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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2001/03/23 15:14:07  thiessen
* load sidechains in CDD's
*
* Revision 1.31  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.30  2001/03/13 01:25:05  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.29  2001/03/09 15:49:04  thiessen
* major changes to add initial update viewer
*
* Revision 1.28  2001/03/06 20:20:50  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.27  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.26  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.25  2001/02/22 00:30:06  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.24  2001/02/13 20:33:49  thiessen
* add information content coloring
*
* Revision 1.23  2001/02/08 23:01:49  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.22  2001/01/18 19:37:28  thiessen
* save structure (re)alignments to asn output
*
* Revision 1.21  2000/12/29 19:23:38  thiessen
* save row order
*
* Revision 1.20  2000/12/22 19:26:40  thiessen
* write cdd output files
*
* Revision 1.19  2000/12/21 23:42:15  thiessen
* load structures from cdd's
*
* Revision 1.18  2000/12/20 23:47:47  thiessen
* load CDD's
*
* Revision 1.17  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.16  2000/11/30 15:49:38  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.15  2000/11/13 18:06:52  thiessen
* working structure re-superpositioning
*
* Revision 1.14  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.13  2000/11/02 16:56:01  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.12  2000/10/16 14:25:48  thiessen
* working alignment fit coloring
*
* Revision 1.11  2000/10/02 23:25:20  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.10  2000/09/20 22:22:27  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.9  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.8  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.7  2000/09/11 14:06:28  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:14  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/03 18:46:48  thiessen
* working generalized sequence viewer
*
* Revision 1.4  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.3  2000/08/28 18:52:41  thiessen
* start unpacking alignments
*
* Revision 1.2  2000/08/27 18:52:20  thiessen
* extract sequence information
*
* Revision 1.1  2000/08/25 18:42:13  thiessen
* rename main object
*
* Revision 1.21  2000/08/25 14:22:00  thiessen
* minor tweaks
*
* Revision 1.20  2000/08/24 23:40:19  thiessen
* add 'atomic ion' labels
*
* Revision 1.19  2000/08/21 17:22:37  thiessen
* add primitive highlighting for testing
*
* Revision 1.18  2000/08/17 18:33:12  thiessen
* minor fixes to StyleManager
*
* Revision 1.17  2000/08/16 14:18:44  thiessen
* map 3-d objects to molecules
*
* Revision 1.16  2000/08/13 02:43:00  thiessen
* added helix and strand objects
*
* Revision 1.15  2000/08/07 14:13:15  thiessen
* added animation frames
*
* Revision 1.14  2000/08/04 22:49:03  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.13  2000/07/27 17:39:21  thiessen
* catch exceptions upon bad file read
*
* Revision 1.12  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.11  2000/07/18 16:50:11  thiessen
* more friendly rotation center setting
*
* Revision 1.10  2000/07/17 22:37:17  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.9  2000/07/17 04:20:49  thiessen
* now does correct structure alignment transformation
*
* Revision 1.8  2000/07/12 23:27:49  thiessen
* now draws basic CPK model
*
* Revision 1.7  2000/07/12 14:11:29  thiessen
* added initial OpenGL rendering engine
*
* Revision 1.6  2000/07/12 02:00:14  thiessen
* add basic wxWindows GUI
*
* Revision 1.5  2000/07/11 13:45:30  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.4  2000/07/01 15:43:50  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.3  2000/06/29 19:17:47  thiessen
* improved atom map
*
* Revision 1.2  2000/06/29 16:46:07  thiessen
* use NCBI streams correctly
*
* Revision 1.1  2000/06/27 20:09:39  thiessen
* initial checkin
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict

#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>

#include "cn3d/asn_reader.hpp"
#include "cn3d/cn3d_main_wxwin.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/show_hide_manager.hpp"
#include "cn3d/show_hide_dialog.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// global strings for various directories
std::string
    workingDir,     // current working directory
    userDir,        // directory of latest user-selected file
    programDir,     // directory where Cn3D executable lives
    dataDir;        // 'data' directory with external data files


// Set the NCBI diagnostic streams to go to this method, which then pastes them
// into a wxWindow. This log window can be closed anytime, but will be hidden,
// not destroyed. More serious errors will bring up a dialog, so that the user
// will get a chance to see the error before the program halts upon a fatal error.

class MsgFrame;

static MsgFrame *logFrame = NULL;
static wxTextCtrl *logText = NULL;

class MsgFrame : public wxFrame
{
public:
    MsgFrame(wxWindow* parent, wxWindowID id, const wxString& title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_FRAME_STYLE, const wxString& name = "frame") :
        wxFrame(parent, id, title, pos, size, style, name) { }
    ~MsgFrame(void) { logFrame = NULL; logText = NULL; }
};

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    std::string errMsg;
    diagMsg.Write(errMsg);

    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace) {
        wxMessageDialog *dlg =
			new wxMessageDialog(NULL, errMsg.c_str(), "Severe Error!",
				wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg->ShowModal();
		delete dlg;
    } else {
        if (!logFrame) {
            logFrame = new MsgFrame(NULL, -1, "Cn3D++ Message Log",
                wxPoint(500, 0), wxSize(500, 500), wxDEFAULT_FRAME_STYLE);
            logFrame->SetSizeHints(150, 100);
            logText = new wxTextCtrl(logFrame, -1, "",
                wxPoint(0,0), wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        }
        // seems to be some upper limit on size, at least under MSW - so delete top of log if too big
        if (logText->GetLastPosition() > 30000) logText->Clear();
        *logText << wxString(errMsg.data(), errMsg.size());
        logFrame->Show(true);
    }
}


// `Main program' equivalent, creating GUI framework
IMPLEMENT_APP(Cn3DApp)

BEGIN_EVENT_TABLE(Cn3DApp, wxApp)
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

bool Cn3DApp::OnInit(void)
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info); // report all messages

    // create the main frame window and messenger
    structureWindow = new Cn3DMainFrame(NULL, "Cn3D++",
        wxPoint(0,0), wxSize(500,500), wxDEFAULT_FRAME_STYLE);

    // set up working directories
    workingDir = userDir = wxGetCwd().c_str();
    if (wxIsAbsolutePath(argv[0]))
        programDir = wxPathOnly(argv[0]).c_str();
    else if (wxPathOnly(argv[0]) == "")
        programDir = workingDir;
    else
        programDir = workingDir + wxFILE_SEP_PATH + programDir;
    TESTMSG("working dir: " << workingDir.c_str());
    TESTMSG("program dir: " << programDir.c_str());

    // read dictionary
    wxString dictFile = wxString(programDir.c_str()) + wxFILE_SEP_PATH + "bstdt.val";
    LoadStandardDictionary(dictFile.c_str());

    // get file name from command line, if present
    if (argc > 2)
        ERR_POST(Fatal << "\nUsage: cn3d [filename]");
    else if (argc == 2) {
        structureWindow->LoadFile(argv[1]);
    }

    return true;
}

int Cn3DApp::OnExit(void)
{
    DeleteStandardDictionary();
	return 0;
}

void Cn3DApp::OnIdle(wxIdleEvent& event)
{
    GlobalMessenger()->ProcessRedraws();

    // call base class OnIdle to continue processing any other system idle-time stuff
    wxApp::OnIdle(event);
}


// data and methods for the main program window (a Cn3DMainFrame)

BEGIN_EVENT_TABLE(Cn3DMainFrame, wxFrame)
    EVT_CLOSE(                                          Cn3DMainFrame::OnCloseWindow)
    EVT_MENU(       MID_EXIT,                           Cn3DMainFrame::OnExit)
    EVT_MENU(       MID_OPEN,                           Cn3DMainFrame::OnOpen)
    EVT_MENU(       MID_SAVE,                           Cn3DMainFrame::OnSave)
    EVT_MENU_RANGE( MID_TRANSLATE,  MID_RESET,          Cn3DMainFrame::OnAdjustView)
    EVT_MENU_RANGE( MID_SHOW_HIDE,  MID_SHOW_SELECTED,  Cn3DMainFrame::OnShowHide)
    EVT_MENU(       MID_REFIT_ALL,                      Cn3DMainFrame::OnAlignStructures)
    EVT_MENU_RANGE( MID_SECSTRUC,   MID_WIREFRAME,      Cn3DMainFrame::OnSetStyle)
    EVT_MENU_RANGE( MID_QLOW,       MID_QHIGH,          Cn3DMainFrame::OnSetQuality)
END_EVENT_TABLE()

Cn3DMainFrame::Cn3DMainFrame(
    wxFrame *parent, const wxString& title, const wxPoint& pos, const wxSize& size, long style) :
    wxFrame(parent, -1, title, pos, size, style | wxTHICK_FRAME),
    glCanvas(NULL)
{
    SetSizeHints(150, 150); // min size

    // File menu
    wxMenuBar *menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_OPEN, "&Open");
    menu->Append(MID_SAVE, "&Save");
    menu->AppendSeparator();
    menu->Append(MID_EXIT, "E&xit");
    menuBar->Append(menu, "&File");

    // View menu
    menu = new wxMenu;
    menu->Append(MID_TRANSLATE, "&Translate");
    menu->Append(MID_ZOOM_IN, "Zoom &In");
    menu->Append(MID_ZOOM_OUT, "Zoom &Out");
    menu->Append(MID_RESET, "&Reset");
    menuBar->Append(menu, "&View");

    // Show-Hide menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_HIDE, "&Pick Structures...");
    menu->Append(MID_SHOW_ALL, "Show &Everything");
    menu->Append(MID_SHOW_DOMAINS, "Show Aligned &Domains");
    menu->Append(MID_SHOW_ALIGNED, "Show &Aligned Residues");
    menu->Append(MID_SHOW_UNALIGNED, "Show &Unaligned Residues");
    menu->Append(MID_SHOW_SELECTED, "Show &Selected Residues");
    menuBar->Append(menu, "Show/&Hide");

    // Structure Alignment menu
    menu = new wxMenu;
    menu->Append(MID_REFIT_ALL, "Recompute &All");
    menuBar->Append(menu, "Structure &Alignments");

    // Style menu
    menu = new wxMenu;
    menu->Append(MID_SECSTRUC, "&Secondary Structure");
    menu->Append(MID_WIREFRAME, "&Wireframe");
    menu->Append(MID_ALIGN, "&Alignment");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_IDENT, "I&dentity");
    subMenu->Append(MID_VARIETY, "&Variety");
    subMenu->Append(MID_WGHT_VAR, "&Weighted Variety");
    subMenu->Append(MID_INFORM, "&Information Content");
    subMenu->Append(MID_FIT, "&Fit");
    menu->Append(MID_CONS, "&Conservation", subMenu);
    menuBar->Append(menu, "&Style");

    // Quality menu
    menu = new wxMenu;
    menu->Append(MID_QLOW, "&Low");
    menu->Append(MID_QMED, "&Medium");
    menu->Append(MID_QHIGH, "&High");
    menuBar->Append(menu, "&Quality");

    SetMenuBar(menuBar);

    // Make a GLCanvas
#ifdef __WXMSW__
    int *gl_attrib = NULL;
#else
    int gl_attrib[20] = {
        GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1, GLX_DEPTH_SIZE, 1,
        GLX_DOUBLEBUFFER, None };
#endif
    glCanvas = new Cn3DGLCanvas(
        this, -1, wxPoint(0, 0), wxSize(400, 400), wxSUNKEN_BORDER, "Cn3DGLCanvas", gl_attrib);
    glCanvas->SetCurrent();

    GlobalMessenger()->AddStructureWindow(this);
    Show(true);
}

Cn3DMainFrame::~Cn3DMainFrame(void)
{
    if (logFrame) {
        logFrame->Destroy();
        logText = NULL;
        logFrame = NULL;
    }
    GlobalMessenger()->RemoveStructureWindow(this);
}

void Cn3DMainFrame::OnCloseWindow(wxCloseEvent& event)
{
    GlobalMessenger()->SequenceWindowsSave();   // give sequence window a chance to save an edited alignment
    SaveDialog(false);                          // give structure window a chance to save data
    Destroy();
}

void Cn3DMainFrame::OnExit(wxCommandEvent& event)
{
    GlobalMessenger()->SequenceWindowsSave();
    SaveDialog(false);
    Destroy();
}

bool Cn3DMainFrame::SaveDialog(bool canCancel)
{
    // check for whether save is necessary
    if (!glCanvas->structureSet || !glCanvas->structureSet->HasDataChanged())
        return true;

    int option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
    if (canCancel) option |= wxCANCEL;

    wxMessageDialog dialog(NULL, "Do you want to save your work to a file?", "", option);
    option = dialog.ShowModal();

    if (option == wxID_CANCEL) return false; // user cancelled this operation

    if (option == wxID_YES)
        OnSave(wxCommandEvent());    // save data

    return true;
}

void Cn3DMainFrame::OnAdjustView(wxCommandEvent& event)
{
    glCanvas->SetCurrent();
    switch (event.GetId()) {
        case MID_TRANSLATE:
            glCanvas->renderer->ChangeView(OpenGLRenderer::eXYTranslateHV, 25, 25);
            break;
        case MID_ZOOM_IN:
            glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomIn);
            break;
        case MID_ZOOM_OUT:
            glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomOut);
            break;
        case MID_RESET:
            glCanvas->renderer->ResetCamera();
            break;
        default: ;
    }
    glCanvas->Refresh(false);
}

void Cn3DMainFrame::OnShowHide(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {

        switch (event.GetId()) {

        case MID_SHOW_HIDE:
        {
            std::vector < std::string > structureNames;
            std::vector < bool > structureVisibilities;
            glCanvas->structureSet->showHideManager->GetShowHideInfo(&structureNames, &structureVisibilities);
            wxString *titles = new wxString[structureNames.size()];
            for (int i=0; i<structureNames.size(); i++) titles[i] = structureNames[i].c_str();

            ShowHideDialog dialog(
                titles,
                &structureVisibilities,
                glCanvas->structureSet->showHideManager,
                NULL, -1, "Show/Hide Structures", wxPoint(400, 50), wxSize(200, 300));
            dialog.Activate();
    //        delete titles;    // apparently deleted by wxWindows
            break;
        }

        case MID_SHOW_ALL:
            glCanvas->structureSet->showHideManager->MakeAllVisible();
            break;
        case MID_SHOW_DOMAINS:
            glCanvas->structureSet->showHideManager->ShowAlignedDomains(glCanvas->structureSet);
            break;
        case MID_SHOW_ALIGNED:
            glCanvas->structureSet->showHideManager->ShowResidues(glCanvas->structureSet, true);
            break;
        case MID_SHOW_UNALIGNED:
            glCanvas->structureSet->showHideManager->ShowResidues(glCanvas->structureSet, false);
            break;
        case MID_SHOW_SELECTED:
            glCanvas->structureSet->showHideManager->ShowSelectedResidues(glCanvas->structureSet);
            break;
        }
    }
}

void Cn3DMainFrame::OnAlignStructures(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->structureSet->alignmentManager->RealignAllSlaveStructures();
        glCanvas->SetCurrent();
        glCanvas->Refresh(false);
    }
}

void Cn3DMainFrame::OnSetStyle(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->SetCurrent();
        switch (event.GetId()) {
            case MID_SECSTRUC:
                glCanvas->structureSet->styleManager->SetToSecondaryStructure();
                break;
            case MID_ALIGN:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eAligned);
                break;
            case MID_IDENT:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eIdentity);
                break;
            case MID_VARIETY:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eVariety);
                break;
            case MID_WGHT_VAR:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eWeightedVariety);
                break;
            case MID_INFORM:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eInformationContent);
                break;
            case MID_FIT:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eFit);
                break;
            case MID_WIREFRAME:
                glCanvas->structureSet->styleManager->SetToWireframe();
                break;
            default: ;
        }
        glCanvas->structureSet->styleManager->CheckStyleSettings(glCanvas->structureSet);
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
}

void Cn3DMainFrame::LoadFile(const char *filename)
{
    glCanvas->SetCurrent();

    // clear old data
    if (glCanvas->structureSet) {
        GlobalMessenger()->RemoveAllHighlights(false);
        delete glCanvas->structureSet;
        glCanvas->structureSet = NULL;
        glCanvas->renderer->AttachStructureSet(NULL);
        glCanvas->Refresh(false);
    }

    if (wxIsAbsolutePath(filename))
        userDir = wxPathOnly(filename).c_str();
    else if (wxPathOnly(filename) == "")
        userDir = workingDir;
    else
        userDir = workingDir + wxFILE_SEP_PATH + wxPathOnly(filename).c_str();
    TESTMSG("user dir: " << userDir.c_str());

    // try to decide if what ASN type this is, and if it's binary or ascii
    CNcbiIstream *inStream = new CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary);
    if (!inStream) {
        ERR_POST(Error << "Cannot open file '" << filename << "' for reading");
        return;
    }
    std::string firstWord;
    *inStream >> firstWord;
    delete inStream;

    static const std::string
        asciiMimeFirstWord = "Ncbi-mime-asn1",
        asciiCDDFirstWord = "Cdd";
    bool isMime = false, isCDD = false, isBinary = true;
    if (firstWord == asciiMimeFirstWord) {
        isMime = true;
        isBinary = false;
    } else if (firstWord == asciiCDDFirstWord) {
        isCDD = true;
        isBinary = false;
    }

    // try to read the file as various ASN types (if it's not clear from the first ascii word).
    // If read is successful, the StructureSet will own the asn data object, to keep it
    // around for output later on
    bool readOK = false;
    std::string err;
    if (!isCDD) {
        TESTMSG("trying to read file '" << filename << "' as " <<
            ((isBinary) ? "binary" : "ascii") << " mime");
        CNcbi_mime_asn1 *mime = new CNcbi_mime_asn1();
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename, *mime, isBinary, err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(mime);
        } else {
            ERR_POST(Warning << "error: " << err);
            delete mime;
        }
    }
    if (!readOK) {
        TESTMSG("trying to read file '" << filename << "' as " <<
            ((isBinary) ? "binary" : "ascii") << " cdd");
        CCdd *cdd = new CCdd();
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename, *cdd, isBinary, err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            wxString cddDir(userDir.c_str());
            cddDir += wxFILE_SEP_PATH;
            TESTMSG("cddDir: '" << cddDir.c_str() << "'");
            glCanvas->structureSet = new StructureSet(cdd, cddDir.c_str());
        } else {
            ERR_POST(Warning << "error: " << err);
            delete cdd;
        }
    }
    if (!readOK) {
        ERR_POST(Error << "File is not a recognized data type (Ncbi-mime-asn1 or Cdd)");
        return;
    }

    glCanvas->renderer->AttachStructureSet(glCanvas->structureSet);
    glCanvas->Refresh(false);
}

void Cn3DMainFrame::OnOpen(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        GlobalMessenger()->SequenceWindowsSave();   // give sequence window a chance to save an edited alignment
        SaveDialog(false);                          // give structure window a chance to save data
    }

    const wxString& filestr = wxFileSelector("Choose a text or binary ASN1 file to open", userDir.c_str());
    if (!filestr.IsEmpty())
        LoadFile(filestr.c_str());
}

void Cn3DMainFrame::OnSave(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    wxString outputFilename = wxFileSelector(
        "Choose a filename for output", userDir.c_str(), "",
        ".val", "ASCII ASN (*.prt)|*.prt|Binary ASN (*.val)|*.val",
        wxSAVE | wxOVERWRITE_PROMPT);
    TESTMSG("save file: '" << outputFilename.c_str() << "'");
    if (!outputFilename.IsEmpty())
        glCanvas->structureSet->SaveASNData(outputFilename.c_str(), (outputFilename.Right(4) == ".val"));
}

void Cn3DMainFrame::OnSetQuality(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_QLOW: glCanvas->renderer->SetLowQuality(); break;
        case MID_QMED: glCanvas->renderer->SetMediumQuality(); break;
        case MID_QHIGH: glCanvas->renderer->SetHighQuality(); break;
        default: ;
    }
    GlobalMessenger()->PostRedrawAllStructures();
}


// data and methods for the GLCanvas used for rendering structures (Cn3DGLCanvas)

BEGIN_EVENT_TABLE(Cn3DGLCanvas, wxGLCanvas)
    EVT_SIZE                (Cn3DGLCanvas::OnSize)
    EVT_PAINT               (Cn3DGLCanvas::OnPaint)
    EVT_CHAR                (Cn3DGLCanvas::OnChar)
    EVT_MOUSE_EVENTS        (Cn3DGLCanvas::OnMouseEvent)
    EVT_ERASE_BACKGROUND    (Cn3DGLCanvas::OnEraseBackground)
END_EVENT_TABLE()

Cn3DGLCanvas::Cn3DGLCanvas(
    wxWindow *parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name, int *gl_attrib) :
    wxGLCanvas(parent, id, pos, size, style, name, gl_attrib),
    structureSet(NULL)
{
    renderer = new OpenGLRenderer;
    SetCurrent();
    font = new wxFont(12, wxSWISS, wxNORMAL, wxBOLD);

#ifdef __WXMSW__
    renderer->SetFont_Windows(font->GetHFONT());
#endif
}

Cn3DGLCanvas::~Cn3DGLCanvas(void)
{
    if (structureSet) delete structureSet;
    if (font) delete font;
    delete renderer;
}

void Cn3DGLCanvas::OnPaint(wxPaintEvent& event)
{
    // This is a dummy, to avoid an endless succession of paint messages.
    // OnPaint handlers must always create a wxPaintDC.
    wxPaintDC dc(this);

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

    SetCurrent();
    renderer->Display();
    SwapBuffers();
}

void Cn3DGLCanvas::OnSize(wxSizeEvent& event)
{
#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

    SetCurrent();
    int width, height;
    GetClientSize(&width, &height);
    renderer->SetSize(width, height);
}

void Cn3DGLCanvas::OnMouseEvent(wxMouseEvent& event)
{
    static bool dragging = false;
    static long last_x, last_y;

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif
    SetCurrent();

    // keep mouse focus while holding down button
    if (event.LeftDown()) CaptureMouse();
    if (event.LeftUp()) ReleaseMouse();

    if (event.LeftIsDown()) {
        if (!dragging) {
            dragging = true;
        } else {
            renderer->ChangeView(OpenGLRenderer::eXYRotateHV,
                event.GetX()-last_x, event.GetY()-last_y);
            Refresh(false);
        }
        last_x = event.GetX();
        last_y = event.GetY();
    } else {
        dragging = false;
    }

    if (event.RightDown()) {
        unsigned int name;
        if (structureSet && renderer->GetSelected(event.GetX(), event.GetY(), &name))
            structureSet->SelectedAtom(name);
    }
}

void Cn3DGLCanvas::OnChar(wxKeyEvent& event)
{
#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif
    SetCurrent();

    switch (event.KeyCode()) {
        case 'a': case 'A': renderer->ShowAllFrames(); Refresh(false); break;
        case WXK_DOWN: renderer->ShowFirstFrame(); Refresh(false); break;
        case WXK_UP: renderer->ShowLastFrame(); Refresh(false); break;
        case WXK_RIGHT: renderer->ShowNextFrame(); Refresh(false); break;
        case WXK_LEFT: renderer->ShowPreviousFrame(); Refresh(false); break;
        default: event.Skip();
    }
}

void Cn3DGLCanvas::OnEraseBackground(wxEraseEvent& event)
{
    // Do nothing, to avoid flashing.
}

END_SCOPE(Cn3D)

