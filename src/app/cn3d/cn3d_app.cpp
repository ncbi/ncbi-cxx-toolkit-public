/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Paul Thiessen
*
* File Description:
*       log and application object for Cn3D
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp> // avoids some 'CurrentTime' conflict later on...
#include <ctools/ctools.h>
#include <serial/objostr.hpp>

#include <algorithm>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/filesys.h>
#include <wx/fs_zip.h>

#include "cn3d/cn3d_app.hpp"
#include "cn3d/structure_window.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/cn3d_glcanvas.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/alignment_manager.hpp"

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d/cn3d.xpm"
#endif

#include <ncbi.h>
#include <ncbienv.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// `Main program' equivalent
IMPLEMENT_APP(Cn3D::Cn3DApp)


BEGIN_SCOPE(Cn3D)

// global strings for various directories - will include trailing path separator character
static string
    workingDir,     // current working directory
    programDir,     // directory where Cn3D executable lives
    dataDir,        // 'data' directory with external data files
    prefsDir;       // application preferences directory
const string& GetWorkingDir(void) { return workingDir; }
const string& GetProgramDir(void) { return programDir; }
const string& GetDataDir(void) { return dataDir; }
const string& GetPrefsDir(void) { return prefsDir; }

// top-level window (the main structure window)
static wxFrame *topWindow = NULL;
wxFrame * GlobalTopWindow(void) { return topWindow; }


// Set the NCBI diagnostic streams to go to this method, which then pastes them
// into a wxWindow. This log window can be closed anytime, but will be hidden,
// not destroyed. More serious errors will bring up a dialog, so that the user
// will get a chance to see the error before the program halts upon a fatal error.

class MsgFrame;

static MsgFrame *logFrame = NULL;
static list < string > backLog;

class MsgFrame : public wxFrame
{
public:
    wxTextCtrl *logText;
    int totalChars;
    MsgFrame(const wxString& title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) :
        wxFrame(GlobalTopWindow(), wxID_HIGHEST + 5, title, pos, size,
            wxDEFAULT_FRAME_STYLE
#if defined(__WXMSW__)
                | wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxFRAME_FLOAT_ON_PARENT
#endif
            )
    {
        totalChars = 0;
        SetIcon(wxICON(cn3d));
    }
    ~MsgFrame(void) { logFrame = NULL; logText = NULL; }
private:
    // need to define a custom close window handler, so that window isn't actually destroyed,
    // just hidden when user closes it with system close button
    void OnCloseWindow(wxCloseEvent& event);
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MsgFrame, wxFrame)
    EVT_CLOSE(MsgFrame::OnCloseWindow)
END_EVENT_TABLE()

void MsgFrame::OnCloseWindow(wxCloseEvent& event)
{
    if (event.CanVeto()) {
        Show(false);
        event.Veto();
    } else {
        Destroy();
        logFrame = NULL;
    }
}

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    string errMsg;
    diagMsg.Write(errMsg);

    // severe errors get a special error dialog
    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace) {
        wxMessageDialog dlg(NULL, errMsg.c_str(), "Severe Error!", wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg.ShowModal();
    }

    // info messages and less severe errors get added to the log
    else {
        if (logFrame) {
            // delete top of log if too big
            if (logFrame->totalChars + errMsg.size() > 100000) {
                logFrame->logText->Clear();
                logFrame->totalChars = 0;
            }
            *(logFrame->logText) << errMsg.c_str();
            logFrame->totalChars += errMsg.size();
            logFrame->logText->ShowPosition(logFrame->logText->GetLastPosition());
        } else {
            // if message window doesn't exist yet, store messages until later
            backLog.push_back(errMsg.c_str());
        }
    }
}

void RaiseLogWindow(void)
{
    if (!logFrame) {
        logFrame = new MsgFrame("Cn3D Message Log", wxPoint(500, 0), wxSize(500, 500));
#ifdef __WXMAC__
        // make empty menu for this window
        wxMenuBar *menuBar = new wxMenuBar;
        logFrame->SetMenuBar(menuBar);
#endif
        logFrame->SetSizeHints(150, 100);
        logFrame->logText = new wxTextCtrl(logFrame, -1, "",
            wxPoint(0,0), logFrame->GetClientSize(), wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        // display any messages received before window created
        while (backLog.size() > 0) {
            *(logFrame->logText) << backLog.front().c_str();
            logFrame->totalChars += backLog.front().size();
            backLog.erase(backLog.begin());
        }
    }
    logFrame->logText->ShowPosition(logFrame->logText->GetLastPosition());
    logFrame->Show(true);
#if defined(__WXMSW__)
    if (logFrame->IsIconized()) logFrame->Maximize(false);
#endif
    logFrame->Raise();
}


BEGIN_EVENT_TABLE(Cn3DApp, wxGLApp)
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

Cn3DApp::Cn3DApp() : wxGLApp()
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info);   // report all messages
    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams

    // C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    if (!InitGLVisual(NULL)) FATALMSG("InitGLVisual failed");
}

void Cn3DApp::InitRegistry(void)
{
    // first set up defaults, then override any/all with stuff from registry file

    // default animation delay and log window startup
    RegistrySetInteger(REG_CONFIG_SECTION, REG_ANIMATION_DELAY, 500);
    RegistrySetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, false);
    RegistrySetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, NO_FAVORITES_FILE);

    // default quality settings
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, 10);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, 8);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, 12);
    RegistrySetBoolean(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, true);
    RegistrySetString(REG_QUALITY_SECTION, REG_PROJECTION_TYPE, "Perspective");

    // default font for OpenGL (structure window)
    wxFont *font = wxFont::New(
#if defined(__WXMSW__)
        12,
#elif defined(__WXGTK__)
        14,
#elif defined(__WXMAC__)
        14,
#endif
        wxSWISS, wxNORMAL, wxBOLD, false);
    if (font && font->Ok())
        RegistrySetString(REG_OPENGL_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO,
			font->GetNativeFontInfoDesc().c_str());
    else
        ERRORMSG("Can't create default structure window font");
    if (font) delete font;

    // default font for sequence viewers
    font = wxFont::New(
#if defined(__WXMSW__)
        10,
#elif defined(__WXGTK__)
        14,
#elif defined(__WXMAC__)
        12,
#endif
        wxROMAN, wxNORMAL, wxNORMAL, false);
    if (font && font->Ok())
        RegistrySetString(REG_SEQUENCE_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO,
			font->GetNativeFontInfoDesc().c_str());
    else
        ERRORMSG("Can't create default sequence window font");
    if (font) delete font;

    // default cache settings
    RegistrySetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, true);
    if (GetPrefsDir().size() > 0)
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetPrefsDir() + "cache");
    else
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetProgramDir() + "cache");
    RegistrySetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, 25);

    // default advanced options
    RegistrySetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, true);
#ifdef __WXGTK__
    RegistrySetString(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH,
        // for launching netscape in a separate window
        "( netscape -noraise -remote 'openURL(<URL>,new-window)' || netscape '<URL>' ) >/dev/null 2>&1 &"
        // for launching netscape in an existing window
//        "( netscape -raise -remote 'openURL(<URL>)' || netscape '<URL>' ) >/dev/null 2>&1 &"
    );
#endif
    RegistrySetInteger(REG_ADVANCED_SECTION, REG_MAX_N_STRUCTS, 10);
    RegistrySetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, 15);

    // load program registry - overriding defaults if present
    LoadRegistry();
}

bool Cn3DApp::OnInit(void)
{
    INFOMSG("Welcome to Cn3D " << CN3D_VERSION_STRING << "!");
    INFOMSG("built " << __DATE__ << " with " << wxVERSION_STRING);

    // set up command line parser
    static const wxCmdLineEntryDesc cmdLineDesc[] = {
        { wxCMD_LINE_SWITCH, "h", "help", "show this help message",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_SWITCH, "r", "readonly", "message file is read-only",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_SWITCH, "i", "imports", "show import window on startup",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_SWITCH, "f", "force", "force saves to same file",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_OPTION, "m", "message", "message file",
            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_NEEDS_SEPARATOR },
        { wxCMD_LINE_PARAM, NULL, NULL, "input file",
            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
        { wxCMD_LINE_NONE }
    };
    commandLine.SetSwitchChars("-");
    commandLine.SetDesc(cmdLineDesc);
    commandLine.SetCmdLine(argc, argv);
    switch (commandLine.Parse()) {
        case 0: TRACEMSG("command line parsed successfully"); break;
        default: return false;  // exit upon either help or syntax error
    }

    // help system loads from zip file
    wxFileSystem::AddHandler(new wxZipFSHandler);

    // set up working directories
    workingDir = wxGetCwd().c_str();
#ifdef __WXGTK__
    if (getenv("CN3D_HOME") != NULL)
        programDir = getenv("CN3D_HOME");
    else
#endif
    if (wxIsAbsolutePath(argv[0]))
        programDir = wxPathOnly(argv[0]).c_str();
    else if (wxPathOnly(argv[0]) == "")
        programDir = workingDir;
    else
        programDir = workingDir + wxFILE_SEP_PATH + wxPathOnly(argv[0]).c_str();
    workingDir = workingDir + wxFILE_SEP_PATH;
    programDir = programDir + wxFILE_SEP_PATH;

    // find or create preferences folder
    wxString localDir;
    wxSplitPath((wxFileConfig::GetLocalFileName("unused")).c_str(), &localDir, NULL, NULL);
    wxString prefsDirLocal = localDir + wxFILE_SEP_PATH + "Cn3D_User";
    wxString prefsDirProg = wxString(programDir.c_str()) + wxFILE_SEP_PATH + "Cn3D_User";
    if (wxDirExists(prefsDirLocal))
        prefsDir = prefsDirLocal.c_str();
    else if (wxDirExists(prefsDirProg))
        prefsDir = prefsDirProg.c_str();
    else {
        // try to create the folder
        if (wxMkdir(prefsDirLocal) && wxDirExists(prefsDirLocal))
            prefsDir = prefsDirLocal.c_str();
        else if (wxMkdir(prefsDirProg) && wxDirExists(prefsDirProg))
            prefsDir = prefsDirProg.c_str();
    }
    if (prefsDir.size() == 0)
        WARNINGMSG("Can't create Cn3D_User folder at either:"
            << "\n    " << prefsDirLocal
            << "\nor  " << prefsDirProg);
    else
        prefsDir += wxFILE_SEP_PATH;

    // set data dir, and register the path in C toolkit registry (mainly for BLAST code)
#ifdef __WXMAC__
    dataDir = programDir + "../Resources/data/";
#else
    dataDir = programDir + "data" + wxFILE_SEP_PATH;
#endif
    Nlm_TransientSetAppParam("ncbi", "ncbi", "data", dataDir.c_str());

    INFOMSG("working dir: " << workingDir.c_str());
    INFOMSG("program dir: " << programDir.c_str());
    INFOMSG("data dir: " << dataDir.c_str());
    INFOMSG("prefs dir: " << prefsDir.c_str());

    // read dictionary
    wxString dictFile = wxString(dataDir.c_str()) + "bstdt.val";
    LoadStandardDictionary(dictFile.c_str());

    // set up registry and favorite styles (must be done before structure window creation)
    InitRegistry();

    // create the main frame window - must be first window created by the app
    structureWindow = new StructureWindow(
        wxString("Cn3D ") + CN3D_VERSION_STRING, wxPoint(0,0), wxSize(500,500));
    SetTopWindow(structureWindow);
    SetExitOnFrameDelete(true); // exit app when structure window is closed
    topWindow = structureWindow;

    // show log if set to do so
    bool showLog = false;
    RegistryGetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, &showLog);
    if (showLog) RaiseLogWindow();

    // get file name from command line, if present
    if (commandLine.GetParamCount() == 1) {
        INFOMSG("command line file: " << commandLine.GetParam(0).c_str());
        structureWindow->LoadFile(commandLine.GetParam(0).c_str(), commandLine.Found("f"));
    } else {
        structureWindow->glCanvas->renderer->AttachStructureSet(NULL);
    }

    // set up messaging file
    wxString messageFilename;
    if (commandLine.Found("m", &messageFilename))
        structureWindow->SetupFileMessenger(
            messageFilename.c_str(), commandLine.Found("r"));

    // give structure window initial focus
    structureWindow->Raise();
    structureWindow->SetFocus();

    // optionally open imports window, but only if any imports present
    if (commandLine.Found("i") && structureWindow->glCanvas->structureSet &&
            structureWindow->glCanvas->structureSet->alignmentManager->NUpdates() > 0)
        structureWindow->glCanvas->structureSet->alignmentManager->ShowUpdateWindow();

    return true;
}

int Cn3DApp::OnExit(void)
{
    SetDiagHandler(NULL, NULL, NULL);
    SetDiagStream(&cout);

    // save registry
    SaveRegistry();

    // remove dictionary
    DeleteStandardDictionary();

    // destroy log
    if (logFrame) logFrame->Destroy();

    return 0;
}

void Cn3DApp::OnIdle(wxIdleEvent& event)
{
    // process pending redraws
    GlobalMessenger()->ProcessRedraws();

    // call base class OnIdle to continue processing any other system idle-time stuff
    wxApp::OnIdle(event);
}

#ifdef __WXMAC__
// special handler for open file apple event
void Cn3DApp::MacOpenFile(const wxString& filename)
{
    if (filename.size() > 0) {
        INFOMSG("apple open event file: " << filename);
        structureWindow->LoadFile(filename);
    }
}
#endif

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2003/10/13 13:23:31  thiessen
* add -i option to show import window
*
* Revision 1.5  2003/07/17 18:47:01  thiessen
* add -f option to force save to same file
*
* Revision 1.4  2003/06/21 20:54:03  thiessen
* explicitly show bottom of log when new text appended
*
* Revision 1.3  2003/05/29 14:34:19  thiessen
* force serial object verification
*
* Revision 1.2  2003/03/13 16:57:14  thiessen
* fix favorites load/save problem
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/
