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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp> // avoids some 'CurrentTime' conflict later on...
#include <serial/objostr.hpp>

#include <objects/mmdb1/Biostruc.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algorithm>
#include <vector>

#include "remove_header_conflicts.hpp"

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/filesys.h>
#include <wx/fs_zip.h>

#include "asn_reader.hpp"
#include "cn3d_app.hpp"
#include "structure_window.hpp"
#include "cn3d_tools.hpp"
#include "structure_set.hpp"
#include "chemical_graph.hpp"
#include "cn3d_glcanvas.hpp"
#include "opengl_renderer.hpp"
#include "messenger.hpp"
#include "alignment_manager.hpp"
#include "cn3d_cache.hpp"
#include "data_manager.hpp"

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d42App.xpm"
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// `Main program' equivalent
IMPLEMENT_APP(Cn3D::Cn3DApp)


BEGIN_SCOPE(Cn3D)

// top-level window (the main structure window)
static wxFrame *topWindow = NULL;
wxFrame * GlobalTopWindow(void) { return topWindow; }

bool IsWindowedMode(void) { return true; }

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

static bool dialogSevereErrors = true;
void SetDialogSevereErrors(bool status)
{
    dialogSevereErrors = status;
}

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    string errMsg;
    diagMsg.Write(errMsg);

    // severe errors get a special error dialog
    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace && dialogSevereErrors) {
        // a bug in wxWidgets makes the error message dialog non-modal w.r.t some windows; this avoids a
        // strange recursion problem that brings up multiple error dialogs (but doesn't fix the underlying problem)
        static bool avoidRecursion = false;
        if (!avoidRecursion) {
            avoidRecursion = true;
            wxMessageDialog dlg(NULL, errMsg.c_str(), "Severe Error!", wxOK | wxCENTRE | wxICON_EXCLAMATION);
            dlg.ShowModal();
            avoidRecursion = false;
        }
    }

    // info messages and less severe errors get added to the log
    else {
        if (logFrame) {
            // delete top of log if too big
            if (logFrame->totalChars + errMsg.size() > 100000) {
                logFrame->logText->Clear();
                logFrame->totalChars = 0;
            }
            logFrame->logText->SetInsertionPoint(logFrame->logText->GetLastPosition());
            *(logFrame->logText) << errMsg.c_str();
            logFrame->totalChars += errMsg.size();
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
        logFrame->Move(500, 20);  //  drop the window below the Mac menubar
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


BEGIN_EVENT_TABLE(Cn3DApp, wxApp /*wxGLApp*/)
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

Cn3DApp::Cn3DApp() : wxApp() /*wxGLApp()*/
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info);   // report all messages
    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
    UnsetDiagTraceFlag(eDPF_Location);
#ifdef _DEBUG
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
#else
    UnsetDiagTraceFlag(eDPF_File);
    UnsetDiagTraceFlag(eDPF_Line);
#endif

    // C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectIStream::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

//    if (!InitGLVisual(NULL))
//        FATALMSG("InitGLVisual failed");
}

bool Cn3DApp::OnInit(void)
{
    INFOMSG("Welcome to Cn3D " << CN3D_VERSION_STRING << "!");
    INFOMSG("built " << __DATE__ << " with wxWidgets " << wxVERSION_NUM_DOT_STRING);

    // set up command line parser
    static const wxCmdLineEntryDesc cmdLineDesc[] = {
        { wxCMD_LINE_SWITCH, "h", "help", "show this help message",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_SWITCH, "r", "readonly", "message file is read-only",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_SWITCH, "i", "imports", "show imports window on startup",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_SWITCH, "n", "noalign", "do not show alignment window on startup",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_SWITCH, "f", "force", "force saves to same file",
            wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_OPTION, "m", "message", "message file",
            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_NEEDS_SEPARATOR },
        { wxCMD_LINE_OPTION, "a", "targetapp", "messaging target application name",
            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_NEEDS_SEPARATOR },
        { wxCMD_LINE_OPTION, "o", "model", "model choice: alpha, single, or PDB",
            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_NEEDS_SEPARATOR },
        { wxCMD_LINE_OPTION, "d", "id", "MMDB/PDB ID to load via network",
            wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_NEEDS_SEPARATOR },
        { wxCMD_LINE_OPTION, "s", "style", "preferred favorite style",
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
    
    // setup dirs
    SetUpWorkingDirectories(argv[0]);

    // read dictionary
    wxString dictFile = wxString(GetDataDir().c_str()) + "bstdt.val";
    LoadStandardDictionary(dictFile.c_str());

    // set up registry and favorite styles (must be done before structure window creation)
    LoadRegistry();

    // create the main frame window - must be first window created by the app
    structureWindow = new StructureWindow(
        wxString("Cn3D ") + CN3D_VERSION_STRING, wxPoint(0,0), wxSize(500,500));

    // On the Mac, drop window down slightly so that the title bar 
    // is not under the menu bar at the top of the screen.
#ifdef __WXMAC__
    structureWindow->Move(0, 20);
#endif

    SetTopWindow(structureWindow);
    SetExitOnFrameDelete(true); // exit app when structure window is closed
    topWindow = structureWindow;

    // show log if set to do so
    bool showLog = false;
    RegistryGetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, &showLog);
    if (showLog) RaiseLogWindow();

    // set preferred style if given
    wxString favStyle;
    if (commandLine.Found("s", &favStyle))
        structureWindow->SetPreferredFavoriteStyle(favStyle.c_str());

    // get model type from -o
    EModel_type model = eModel_type_other;
    wxString modelStr;
    if (commandLine.Found("o", &modelStr)) {
        if (modelStr == "alpha")
            model = eModel_type_ncbi_backbone;
        else if (modelStr == "single")
            model = eModel_type_ncbi_all_atom;
        else if (modelStr == "PDB")
            model = eModel_type_pdb_model;
        else
            ERRORMSG("Model type (-o) must be one of alpha|single|PDB");
    }

    // load file given on command line
    if (commandLine.GetParamCount() == 1) {
        wxString filename = commandLine.GetParam(0).c_str();
        INFOMSG("command line file: " << filename.c_str());
        // if -o is present, assume param is a Biostruc file
        if (model != eModel_type_other) {   // -o present
            CNcbi_mime_asn1 *mime = CreateMimeFromBiostruc(filename.c_str(), model);
            if (mime)
                structureWindow->LoadData(NULL, commandLine.Found("f"), commandLine.Found("n"), mime);
        } else {
            structureWindow->LoadData(filename.c_str(), commandLine.Found("f"), commandLine.Found("n"));
        }
    }

    // if no file passed but there is -o, see if there's a -d parameter (MMDB/PDB ID to fetch)
    else if (model != eModel_type_other) {  // -o present
        wxString id;
        if (commandLine.Found("d", &id)) {
            CNcbi_mime_asn1 *mime = LoadStructureViaCache(id.c_str(), model);
            if (mime)
                structureWindow->LoadData(NULL, commandLine.Found("f"), commandLine.Found("n"), mime);
        } else {
            ERRORMSG("-o requires either -d or Biostruc file name");
        }
    }

    // if no structure loaded, show the logo
    if (!structureWindow->glCanvas->structureSet) {
        structureWindow->glCanvas->renderer->AttachStructureSet(NULL);
        structureWindow->glCanvas->Refresh(false);
    }

    // set up messaging file communication
    wxString messageFilename;
    if (commandLine.Found("m", &messageFilename)) {
        wxString messageApp;
        if (!commandLine.Found("a", &messageApp))
            messageApp = "Listener";
        structureWindow->SetupFileMessenger(
            messageFilename.c_str(), messageApp.c_str(), commandLine.Found("r"));
    }

    // optionally open imports window, but only if any imports present
    if (commandLine.Found("i") && structureWindow->glCanvas->structureSet &&
            structureWindow->glCanvas->structureSet->alignmentManager->NUpdates() > 0)
        structureWindow->glCanvas->structureSet->alignmentManager->ShowUpdateWindow();

    // give structure window initial focus
    structureWindow->Raise();
    structureWindow->SetFocus();
    
    // hack to fix some platforms that start with initially blank window
    structureWindow->glCanvas->FakeOnSize();

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
        structureWindow->LoadData(filename, false, commandLine.Found("n"));
    }
}
#endif

END_SCOPE(Cn3D)
