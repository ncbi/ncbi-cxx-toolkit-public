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
*       main windows (structure and log) and application object for Cn3D
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.67  2001/08/15 20:32:26  juran
* Define attribList for Mac OS.
*
* Revision 1.66  2001/08/14 17:18:22  thiessen
* add user font selection, store in registry
*
* Revision 1.65  2001/08/13 22:30:58  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.64  2001/08/10 15:01:57  thiessen
* fill out shortcuts; add update show/hide menu
*
* Revision 1.63  2001/08/09 23:14:13  thiessen
* fixes for MIPSPro and Mac compilers
*
* Revision 1.62  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.61  2001/08/06 20:22:00  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.60  2001/08/03 13:41:33  thiessen
* add registry and style favorites
*
* Revision 1.59  2001/07/23 20:09:23  thiessen
* add regex pattern search
*
* Revision 1.58  2001/07/12 17:35:15  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.57  2001/07/10 16:39:54  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.56  2001/07/04 19:39:16  thiessen
* finish user annotation system
*
* Revision 1.55  2001/06/29 18:13:57  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.54  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.53  2001/06/15 14:06:40  thiessen
* save/load asn styles now complete
*
* Revision 1.52  2001/06/14 17:45:10  thiessen
* progress in styles<->asn ; add structure limits
*
* Revision 1.51  2001/06/07 19:05:37  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.50  2001/06/02 19:14:08  thiessen
* Maximize() not implemented in wxGTK
*
* Revision 1.49  2001/05/31 18:47:07  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.48  2001/05/31 14:32:32  thiessen
* tweak font handling
*
* Revision 1.47  2001/05/25 18:15:13  thiessen
* another programDir fix
*
* Revision 1.46  2001/05/25 18:10:40  thiessen
* programDir path fix
*
* Revision 1.45  2001/05/25 15:18:23  thiessen
* fixes for visual id settings in wxGTK
*
* Revision 1.44  2001/05/24 19:13:23  thiessen
* fix to compile on SGI
*
* Revision 1.43  2001/05/22 19:09:30  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.42  2001/05/21 22:06:51  thiessen
* fix initial glcanvas size bug
*
* Revision 1.41  2001/05/18 19:17:22  thiessen
* get gl context working in GTK
*
* Revision 1.40  2001/05/17 18:34:25  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.39  2001/05/15 23:48:36  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.38  2001/05/15 17:33:53  thiessen
* log window stays hidden when closed
*
* Revision 1.37  2001/05/15 14:57:54  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* Revision 1.36  2001/05/11 13:45:06  thiessen
* set up data directory
*
* Revision 1.35  2001/05/11 02:10:41  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.34  2001/05/02 13:46:28  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.33  2001/03/30 14:43:40  thiessen
* show threader scores in status line; misc UI tweaks
*
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
#include <objects/cn3d/Cn3d_style_settings_set.hpp>

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
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/multitext_dialog.hpp"
#include "cn3d/cdd_annot_dialog.hpp"
#include "cn3d/preferences_dialog.hpp"

#include <wx/file.h>
#include <wx/fontdlg.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// from C-toolkit ncbienv.h - for setting registry parameter; used here to avoid having to
// bring in lots of C-toolkit headers
extern "C" {
    extern int Nlm_TransientSetAppParam(const char* file, const char* section, const char* type, const char* value);
}


// `Main program' equivalent, creating GUI framework
IMPLEMENT_APP(Cn3D::Cn3DApp)


BEGIN_SCOPE(Cn3D)

// global strings for various directories - will include trailing path separator character
std::string
    workingDir,     // current working directory
    userDir,        // directory of latest user-selected file
    programDir,     // directory where Cn3D executable lives
    dataDir;        // 'data' directory with external data files
const std::string& GetWorkingDir(void) { return workingDir; }
const std::string& GetUserDir(void) { return userDir; }
const std::string& GetProgramDir(void) { return programDir; }
const std::string& GetDataDir(void) { return dataDir; }

// top-level window (the main structure window)
static wxFrame *topWindow = NULL;
wxFrame * GlobalTopWindow(void) { return topWindow; }

// global program registry
static CNcbiRegistry registry;
static std::string registryFile;
static bool registryChanged = false;

// stuff for style favorites
static CCn3d_style_settings_set favoriteStyles;
static bool favoriteStylesChanged = false;


// Set the NCBI diagnostic streams to go to this method, which then pastes them
// into a wxWindow. This log window can be closed anytime, but will be hidden,
// not destroyed. More serious errors will bring up a dialog, so that the user
// will get a chance to see the error before the program halts upon a fatal error.

class MsgFrame;

static MsgFrame *logFrame = NULL;

class MsgFrame : public wxFrame
{
public:
    MsgFrame(const wxString& title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) :
        wxFrame(topWindow, wxID_HIGHEST + 5, title, pos, size,
            wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT) { }
    ~MsgFrame(void) { logFrame = NULL; logText = NULL; }
    wxTextCtrl *logText;
private:
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
    }
}

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    std::string errMsg;
    diagMsg.Write(errMsg);

    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace) {
        wxMessageDialog dlg(NULL, errMsg.c_str(), "Severe Error!", wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg.ShowModal();
    } else {
        if (!logFrame) {
            logFrame = new MsgFrame("Cn3D++ Message Log", wxPoint(500, 0), wxSize(500, 500));
            logFrame->SetSizeHints(150, 100);
            logFrame->logText = new wxTextCtrl(logFrame, -1, "",
                wxPoint(0,0), wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        }
        // seems to be some upper limit on size, at least under MSW - so delete top of log if too big
        if (logFrame->logText->GetLastPosition() > 30000) logFrame->logText->Clear();
        *(logFrame->logText) << wxString(errMsg.data(), errMsg.size());
    }
}

void RaiseLogWindow(void)
{
    logFrame->logText->ShowPosition(logFrame->logText->GetLastPosition());
    logFrame->Show(true);
#if defined(__WXMSW__)
    if (logFrame->IsIconized()) logFrame->Maximize(false);
#endif
    logFrame->Raise();
}


static wxString GetFavoritesFile(bool forRead)
{
    // try to get value from registry
    wxString file = registry.Get(REG_CONFIG_SECTION, REG_FAVORITES_NAME).c_str();

    // if not set, ask user for a folder, then set in registry
    if (file.size() == 0) {
        file = wxFileSelector("Select a file for favorites:",
            dataDir.c_str(), "Favorites", "", "*.*",
            forRead ? wxOPEN : wxSAVE | wxOVERWRITE_PROMPT);

        if (file.size() > 0) {
            if (!registry.Set(REG_CONFIG_SECTION, REG_FAVORITES_NAME, file.c_str(),
                    CNcbiRegistry::ePersistent | CNcbiRegistry::eTruncate))
                ERR_POST(Error << "Error setting favorites file in registry");
            registryChanged = true;
        }
    }

    return file;
}

static bool LoadFavorites(void)
{
    std::string favoritesFile = registry.Get(REG_CONFIG_SECTION, REG_FAVORITES_NAME);
    if (favoritesFile.size() > 0) {
        favoriteStyles.Reset();
        if (wxFile::Exists(favoritesFile.c_str())) {
            TESTMSG("loading favorites from " << favoritesFile);
            std::string err;
            if (ReadASNFromFile(favoritesFile.c_str(), favoriteStyles, false, err)) {
                favoriteStylesChanged = false;
                return true;
            }
        } else {
            ERR_POST(Warning << "Favorites file does not exist: " << favoritesFile);
            registry.Set(REG_CONFIG_SECTION, REG_FAVORITES_NAME, "", CNcbiRegistry::ePersistent);
            registryChanged = true;
        }
    }
    return false;
}

static void SaveFavorites(void)
{
    if (favoriteStylesChanged) {
        int choice = wxMessageBox("Do you want to save changes to your current Favorites file?",
            "Save favorites?", wxYES_NO);
        if (choice == wxYES) {
            wxString favoritesFile = GetFavoritesFile(false);
            if (favoritesFile.size() > 0) {
                std::string err;
                if (!WriteASNToFile(favoritesFile.c_str(), favoriteStyles, false, err))
                    ERR_POST(Error << "Error saving Favorites to " << favoritesFile << '\n' << err);
                favoriteStylesChanged = false;
            }
        }
    }
}


BEGIN_EVENT_TABLE(Cn3DApp, wxApp)
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

Cn3DApp::Cn3DApp() : wxApp()
{
    // try to force all windows to use best (TrueColor) visuals
    SetUseBestVisual(true);
}

#define SET_DEFAULT_INTEGER_REGISTRY_VALUE(section, name, defval) \
    if (!RegistryIsValidInteger((section), (name))) \
        RegistrySetInteger((section), (name), (defval));

#define SET_DEFAULT_BOOLEAN_REGISTRY_VALUE(section, name, defval) \
    if (!RegistryIsValidBoolean((section), (name))) \
        RegistrySetBoolean((section), (name), (defval), true);

bool Cn3DApp::OnInit(void)
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info); // report all messages

    TESTMSG("Welcome to Cn3D++! (built " << __DATE__ << ')');
    RaiseLogWindow();

    // set up working directories
    workingDir = userDir = wxGetCwd().c_str();
    if (wxIsAbsolutePath(argv[0]))
        programDir = wxPathOnly(argv[0]).c_str();
    else if (wxPathOnly(argv[0]) == "")
        programDir = workingDir;
    else
        programDir = workingDir + wxFILE_SEP_PATH + wxPathOnly(argv[0]).c_str();
    workingDir = workingDir + wxFILE_SEP_PATH;
    programDir = programDir + wxFILE_SEP_PATH;

    // set data dir, and register the path in C toolkit registry (mainly for BLAST code)
    dataDir = programDir + "data" + wxFILE_SEP_PATH;
    Nlm_TransientSetAppParam("ncbi", "ncbi", "data", dataDir.c_str());

    TESTMSG("working dir: " << workingDir.c_str());
    TESTMSG("program dir: " << programDir.c_str());
    TESTMSG("data dir: " << dataDir.c_str());

    // load program registry
    registryFile = dataDir + "cn3d.ini";
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(registryFile.c_str(), IOS_BASE::in));
    if (*iniIn) {
        TESTMSG("loading program registry " << registryFile);
        registry.Read(*iniIn);
    }

    // favorite styles
    LoadFavorites();

    // initial quality settings if not already present in registry
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, 10);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, 8);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, 6);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, 6);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, 6);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, 12);
    SET_DEFAULT_BOOLEAN_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, true);

    // initial font for OpenGL (structure window)
#if defined(__WXMSW__)
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, 12);
#elif defined(__WXGTK__)
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, 14);
#endif
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_OPENGL_FONT_SECTION, REG_FONT_FAMILY, wxSWISS);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_OPENGL_FONT_SECTION, REG_FONT_STYLE, wxNORMAL);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_OPENGL_FONT_SECTION, REG_FONT_WEIGHT, wxBOLD);
    SET_DEFAULT_BOOLEAN_REGISTRY_VALUE(REG_OPENGL_FONT_SECTION, REG_FONT_UNDERLINED, false);

    // initial font for sequence viewers
#if defined(__WXMSW__)
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_SEQUENCE_FONT_SECTION, REG_FONT_SIZE, 10);
#elif defined(__WXGTK__)
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_SEQUENCE_FONT_SECTION, REG_FONT_SIZE, 14);
#endif
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_SEQUENCE_FONT_SECTION, REG_FONT_FAMILY, wxROMAN);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_SEQUENCE_FONT_SECTION, REG_FONT_STYLE, wxNORMAL);
    SET_DEFAULT_INTEGER_REGISTRY_VALUE(REG_SEQUENCE_FONT_SECTION, REG_FONT_WEIGHT, wxNORMAL);
    SET_DEFAULT_BOOLEAN_REGISTRY_VALUE(REG_SEQUENCE_FONT_SECTION, REG_FONT_UNDERLINED, false);

    // read dictionary
    wxString dictFile = wxString(dataDir.c_str()) + "bstdt.val";
    LoadStandardDictionary(dictFile.c_str());

    // create the main frame window
    structureWindow = new Cn3DMainFrame("Cn3D++", wxPoint(0,0), wxSize(500,500));
    SetTopWindow(structureWindow);

    // get file name from command line, if present
    if (argc > 2)
        ERR_POST(Fatal << "\nUsage: cn3d [filename]");
    else if (argc == 2)
        structureWindow->LoadFile(argv[1]);
    else
        structureWindow->glCanvas->renderer->AttachStructureSet(NULL);

    return true;
}

int Cn3DApp::OnExit(void)
{
    SetDiagHandler(NULL, NULL, NULL);
    SetDiagStream(&cout);

    // save registry
    if (registryChanged) {
        auto_ptr<CNcbiOfstream> iniOut(new CNcbiOfstream(registryFile.c_str(), IOS_BASE::out));
        if (*iniOut) {
            TESTMSG("saving program registry " << registryFile);
            registry.Write(*iniOut);
        }
    }

    // remove dictionary
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

const int Cn3DMainFrame::UNLIMITED_STRUCTURES = -2;

BEGIN_EVENT_TABLE(Cn3DMainFrame, wxFrame)
    EVT_CLOSE     (                                         Cn3DMainFrame::OnCloseWindow)
    EVT_MENU      (MID_EXIT,                                Cn3DMainFrame::OnExit)
    EVT_MENU      (MID_OPEN,                                Cn3DMainFrame::OnOpen)
    EVT_MENU      (MID_SAVE,                                Cn3DMainFrame::OnSave)
    EVT_MENU_RANGE(MID_ZOOM_IN,  MID_ALL_FRAMES,            Cn3DMainFrame::OnAdjustView)
    EVT_MENU_RANGE(MID_SHOW_HIDE,  MID_SHOW_SELECTED,       Cn3DMainFrame::OnShowHide)
    EVT_MENU      (MID_REFIT_ALL,                           Cn3DMainFrame::OnAlignStructures)
    EVT_MENU_RANGE(MID_EDIT_STYLE, MID_ANNOTATE,            Cn3DMainFrame::OnSetStyle)
    EVT_MENU_RANGE(MID_ADD_FAVORITE, MID_FAVORITES_FILE,    Cn3DMainFrame::OnEditFavorite)
    EVT_MENU_RANGE(MID_FAVORITES_BEGIN, MID_FAVORITES_END,  Cn3DMainFrame::OnSelectFavorite)
    EVT_MENU_RANGE(MID_SHOW_LOG,   MID_SHOW_SEQ_V,          Cn3DMainFrame::OnShowWindow)
    EVT_MENU_RANGE(MID_EDIT_CDD_DESCR, MID_ANNOT_CDD,       Cn3DMainFrame::OnCDD)
    EVT_MENU      (MID_PREFERENCES,                         Cn3DMainFrame::OnPreferences)
    EVT_MENU_RANGE(MID_OPENGL_FONT, MID_SEQUENCE_FONT,      Cn3DMainFrame::OnSetFont)
    EVT_MENU      (MID_LIMIT_STRUCT,                        Cn3DMainFrame::OnLimit)
END_EVENT_TABLE()

static void SetupFavoritesMenu(wxMenu *favoritesMenu)
{
    int i;
    for (i=Cn3DMainFrame::MID_FAVORITES_BEGIN; i<=Cn3DMainFrame::MID_FAVORITES_END; i++) {
        wxMenuItem *item = favoritesMenu->FindItem(i);
        if (item) favoritesMenu->Delete(item);
    }

    CCn3d_style_settings_set::Tdata::const_iterator f, fe = favoriteStyles.Get().end();
    for (f=favoriteStyles.Get().begin(), i=0; f!=fe; f++, i++)
        favoritesMenu->Append(Cn3DMainFrame::MID_FAVORITES_BEGIN + i, (*f)->GetName().c_str());
}

Cn3DMainFrame::Cn3DMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
    wxFrame(NULL, wxID_HIGHEST + 1, title, pos, size, wxDEFAULT_FRAME_STYLE | wxTHICK_FRAME),
    glCanvas(NULL), structureLimit(UNLIMITED_STRUCTURES)
{
    topWindow = this;
    SetSizeHints(150, 150); // min size

    // File menu
    menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_OPEN, "&Open");
    menu->Append(MID_SAVE, "&Save");
    menu->AppendSeparator();
    menu->Append(MID_PREFERENCES, "&Preferences...");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_OPENGL_FONT, "S&tructure Window");
    subMenu->Append(MID_SEQUENCE_FONT, "Se&quence Windows");
    menu->Append(MID_FONTS, "Set &Fonts...", subMenu);
    menu->Append(MID_LIMIT_STRUCT, "&Limit Structures");
    menu->AppendSeparator();
    menu->Append(MID_EXIT, "E&xit");
    menuBar->Append(menu, "&File");

    // View menu
    menu = new wxMenu;
    menu->Append(MID_ZOOM_IN, "Zoom &In\tz");
    menu->Append(MID_ZOOM_OUT, "Zoom &Out\tx");
    menu->Append(MID_RESET, "&Reset");
    menu->AppendSeparator();
    menu->Append(MID_NEXT_FRAME, "&Next Frame\tRight");
    menu->Append(MID_PREV_FRAME, "Pre&vious Frame\tLeft");
    menu->Append(MID_FIRST_FRAME, "&First Frame\tDown");
    menu->Append(MID_LAST_FRAME, "&Last Frame\tUp");
    menu->Append(MID_ALL_FRAMES, "&All Frames\ta");
    menuBar->Append(menu, "&View");

    // Show-Hide menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_HIDE, "&Pick Structures...");
    menu->AppendSeparator();
    menu->Append(MID_SHOW_ALL, "Show &Everything");
    menu->Append(MID_SHOW_DOMAINS, "Show Aligned &Domains");
    menu->Append(MID_SHOW_ALIGNED, "Show &Aligned Residues");
    menu->Append(MID_SHOW_SELECTED, "Show &Selected Residues");
    subMenu = new wxMenu;
    subMenu->Append(MID_SHOW_UNALIGNED_ALL, "Show &All");
    subMenu->Append(MID_SHOW_UNALIGNED_ALN_DOMAIN, "Show in Aligned &Domains");
    menu->Append(MID_SHOW_UNALIGNED, "&Unaligned Residues", subMenu);
    menuBar->Append(menu, "Show/&Hide");

    // Structure Alignment menu
    menu = new wxMenu;
    menu->Append(MID_REFIT_ALL, "Recompute &Alignments");
    menuBar->Append(menu, "S&tructures");

    // Style menu
    menu = new wxMenu;
    menu->Append(MID_EDIT_STYLE, "Edit &Global Style");
    // favorites
    favoritesMenu = new wxMenu;
    favoritesMenu->Append(Cn3DMainFrame::MID_ADD_FAVORITE, "&Add/Replace");
    favoritesMenu->Append(Cn3DMainFrame::MID_REMOVE_FAVORITE, "&Remove");
    favoritesMenu->Append(Cn3DMainFrame::MID_FAVORITES_FILE, "&Change File");
    favoritesMenu->AppendSeparator();
    SetupFavoritesMenu(favoritesMenu);
    menu->Append(MID_FAVORITES, "&Favorites", favoritesMenu);
    // rendering shortcuts
    subMenu = new wxMenu;
    subMenu->Append(MID_WORM, "&Worms");
    subMenu->Append(MID_TUBE, "&Tubes");
    subMenu->Append(MID_WIRE, "Wir&e");
    subMenu->Append(MID_BNS, "&Ball and Stick");
    subMenu->Append(MID_SPACE, "&Space Fill");
    subMenu->AppendSeparator();
    subMenu->Append(MID_SC_TOGGLE, "&Toggle Sidechains");
    menu->Append(MID_RENDER, "&Rendering Shortcuts", subMenu);
    // coloring shortcuts
    subMenu = new wxMenu;
    subMenu->Append(MID_SECSTRUC, "&Secondary Structure");
    subMenu->Append(MID_ALIGNED, "&Aligned");
    wxMenu *subMenu2 = new wxMenu;
    subMenu2->Append(MID_IDENTITY, "I&dentity");
    subMenu2->Append(MID_VARIETY, "&Variety");
    subMenu2->Append(MID_WGHT_VAR, "&Weighted Variety");
    subMenu2->Append(MID_INFO, "&Information Content");
    subMenu2->Append(MID_FIT, "&Fit");
    subMenu->Append(MID_CONS, "Sequence &Conservation", subMenu2);
    subMenu->Append(MID_OBJECT, "&Object");
    subMenu->Append(MID_DOMAIN, "&Domain");
    subMenu->Append(MID_MOLECULE, "&Molecule");
    subMenu->Append(MID_HYDROPHOB, "&Hydrophobicity");
    subMenu->Append(MID_TEMP, "&Temperature");
    subMenu->Append(MID_ELEMENT, "&Element");
    menu->Append(MID_COLORS, "&Coloring Shortcuts", subMenu);
    //annotate
    menu->AppendSeparator();
    menu->Append(MID_ANNOTATE, "A&nnotate");
    menuBar->Append(menu, "&Style");

    // Window menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_SEQ_V, "Show &Sequence Viewer");
    menu->Append(MID_SHOW_LOG, "Show Message &Log");
    menuBar->Append(menu, "&Window");

    // CDD menu
    menu = new wxMenu;
    menu->Append(MID_EDIT_CDD_DESCR, "Edit &Description");
    menu->Append(MID_EDIT_CDD_NOTES, "Edit &Notes");
    menu->Append(MID_ANNOT_CDD, "&Annotate");
    menuBar->Append(menu, "&CDD");

    // accelerators for special keys
    wxAcceleratorEntry entries[4];
    entries[0].Set(wxACCEL_NORMAL, WXK_RIGHT, MID_NEXT_FRAME);
    entries[1].Set(wxACCEL_NORMAL, WXK_LEFT, MID_PREV_FRAME);
    entries[2].Set(wxACCEL_NORMAL, WXK_DOWN, MID_FIRST_FRAME);
    entries[3].Set(wxACCEL_NORMAL, WXK_UP, MID_LAST_FRAME);
    wxAcceleratorTable accel(4, entries);
    SetAcceleratorTable(accel);

    SetMenuBar(menuBar);
    menuBar->EnableTop(menuBar->FindMenu("&CDD"), false);

    // Make a GLCanvas
#if defined(__WXMSW__)
    int *attribList = NULL;
#elif defined(__WXGTK__)
    int *attribList = NULL;
/*
    int attribList[20] = {
        WX_GL_DOUBLEBUFFER,
        WX_GL_RGBA, WX_GL_MIN_RED, 1, WX_GL_MIN_GREEN, 1, WX_GL_MIN_BLUE, 1,
        WX_GL_ALPHA_SIZE, 0, WX_GL_DEPTH_SIZE, 1,
        None
    };
*/
#else
#warning need to define GL attrib list
	int *attribList = NULL;
#endif
    glCanvas = new Cn3DGLCanvas(this, attribList);

    GlobalMessenger()->AddStructureWindow(this);
    Show(true);

    // set initial font
    glCanvas->SetupFontFromRegistry();
}

Cn3DMainFrame::~Cn3DMainFrame(void)
{
    if (logFrame) {
        logFrame->Destroy();
        logFrame = NULL;
    }
}

void Cn3DMainFrame::OnSetFont(wxCommandEvent& event)
{
    std::string section;
    if (event.GetId() == MID_OPENGL_FONT)
        section = REG_OPENGL_FONT_SECTION;
    else if (event.GetId() == MID_SEQUENCE_FONT)
        section = REG_SEQUENCE_FONT_SECTION;
    else
        return;

    // get initial font info from registry, and create wxFont
    int size, family, style, weight;
    bool underlined;
    if (!RegistryGetInteger(section, REG_FONT_SIZE, &size) ||
        !RegistryGetInteger(section, REG_FONT_FAMILY, &family) ||
        !RegistryGetInteger(section, REG_FONT_STYLE, &style) ||
        !RegistryGetInteger(section, REG_FONT_WEIGHT, &weight) ||
        !RegistryGetBoolean(section, REG_FONT_UNDERLINED, &underlined))
    {
        ERR_POST(Error << "Cn3DMainFrame::OnSetFont() - error setting up initial font");
        return;
    }
    wxFont initialFont(size, family, style, weight, underlined);
    wxFontData initialFontData;
    initialFontData.SetInitialFont(initialFont);

    // bring up font chooser dialog
    wxFontDialog dialog(this, &initialFontData);
    int result = dialog.ShowModal();

    // if user selected a font
    if (result == wxID_OK) {

        // set registry values appropriately
        wxFontData& fontData = dialog.GetFontData();
        wxFont font = fontData.GetChosenFont();
        if (!RegistrySetInteger(section, REG_FONT_SIZE, font.GetPointSize()) ||
            !RegistrySetInteger(section, REG_FONT_FAMILY, font.GetFamily()) ||
            !RegistrySetInteger(section, REG_FONT_STYLE, font.GetStyle()) ||
            !RegistrySetInteger(section, REG_FONT_WEIGHT, font.GetWeight()) ||
            !RegistrySetBoolean(section, REG_FONT_UNDERLINED, font.GetUnderlined(), true))
        {
            ERR_POST(Error << "Cn3DMainFrame::OnSetFont() - error setting registry data");
            return;
        }

        // call font setup
        TESTMSG("setting new font");
        if (event.GetId() == MID_OPENGL_FONT) {
            glCanvas->SetupFontFromRegistry();
            GlobalMessenger()->PostRedrawAllStructures();
        } else if (event.GetId() == MID_SEQUENCE_FONT) {
            GlobalMessenger()->NewSequenceViewerFont();
        }
    }
}

void Cn3DMainFrame::OnEditFavorite(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    if (event.GetId() == MID_ADD_FAVORITE) {
        // get name from user
        wxString name = wxGetTextFromUser("Enter a name for this style:", "Input name", "", this);
        if (name.size() == 0) return;

        // replace style of same name
        CCn3d_style_settings *settings;
        CCn3d_style_settings_set::Tdata::iterator f, fe = favoriteStyles.Set().end();
        for (f=favoriteStyles.Set().begin(); f!=fe; f++) {
            if (Stricmp((*f)->GetName().c_str(), name.c_str()) == 0) {
                settings = f->GetPointer();
                break;
            }
        }
        // else add style to list
        if (f == favoriteStyles.Set().end()) {
            if (favoriteStyles.Set().size() >= MID_FAVORITES_END - MID_FAVORITES_BEGIN + 1) {
                ERR_POST(Error << "Already have max # Favorites");
                return;
            } else {
                CRef < CCn3d_style_settings > ref(settings = new CCn3d_style_settings());
                favoriteStyles.Set().push_back(ref);
            }
        }

        // put in data from global style
        if (!glCanvas->structureSet->styleManager->GetGlobalStyle().SaveSettingsToASN(settings))
            ERR_POST(Error << "Error converting global style to asn");
        settings->SetName(name.c_str());
        favoriteStylesChanged = true;
    }

    else if (event.GetId() == MID_REMOVE_FAVORITE) {
        wxString *choices = new wxString[favoriteStyles.Set().size()];
        CCn3d_style_settings_set::Tdata::iterator f, fe = favoriteStyles.Set().end();
        int i = 0;
        for (f=favoriteStyles.Set().begin(); f!=fe; f++)
            choices[i++] = (*f)->GetName().c_str();
        int picked = wxGetSingleChoiceIndex("Choose a style to remove from the Favorites list:",
            "Select for removal", favoriteStyles.Set().size(), choices, this);
        if (picked < 0 || picked >= favoriteStyles.Set().size()) return;
        for (f=favoriteStyles.Set().begin(), i=0; f!=fe; f++, i++) {
            if (i == picked) {
                favoriteStyles.Set().erase(f);
                favoriteStylesChanged = true;
                break;
            }
        }
    }

    else if (event.GetId() == MID_FAVORITES_FILE) {
        SaveFavorites();
        favoriteStyles.Reset();
        registry.Set(REG_CONFIG_SECTION, REG_FAVORITES_NAME, "", CNcbiRegistry::ePersistent);
        registryChanged = true;
        wxString newFavorites = GetFavoritesFile(true);
        if (newFavorites.size() > 0 && wxFile::Exists(newFavorites.c_str())) {
            if (!LoadFavorites())
                ERR_POST(Error << "Error loading Favorites from " << newFavorites.c_str());
        }
    }

    // update menu
    SetupFavoritesMenu(favoritesMenu);
}

void Cn3DMainFrame::OnSelectFavorite(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    if (event.GetId() >= MID_FAVORITES_BEGIN && event.GetId() <= MID_FAVORITES_END) {
        int index = event.GetId() - MID_FAVORITES_BEGIN;
        CCn3d_style_settings_set::Tdata::const_iterator f, fe = favoriteStyles.Get().end();
        int i = 0;
        for (f=favoriteStyles.Get().begin(); f!=fe; f++, i++) {
            if (i == index) {
                TESTMSG("using favorite: " << (*f)->GetName());
                glCanvas->structureSet->styleManager->SetGlobalStyle(**f);
                break;
            }
        }
    }
}

void Cn3DMainFrame::OnShowWindow(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SHOW_LOG:
            RaiseLogWindow();
            break;
        case MID_SHOW_SEQ_V:
            GlobalMessenger()->PostRedrawAllSequenceViewers();
            break;
    }
}

void Cn3DMainFrame::OnCloseWindow(wxCloseEvent& event)
{
    Command(MID_EXIT);
}

void Cn3DMainFrame::OnExit(wxCommandEvent& event)
{
    GlobalMessenger()->RemoveStructureWindow(this); // don't bother with any redraws since we're exiting
    GlobalMessenger()->SequenceWindowsSave();       // save any edited alignment and updates first
    SaveDialog(false);                              // give structure window a chance to save data
    SaveFavorites();
    Destroy();
}

void Cn3DMainFrame::OnPreferences(wxCommandEvent& event)
{
    PreferencesDialog dialog(this);
    int result = dialog.ShowModal();
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

    if (option == wxID_YES) {
        wxCommandEvent event;
        OnSave(event);    // save data
    }

    return true;
}

void Cn3DMainFrame::OnCDD(wxCommandEvent& event)
{
    if (!glCanvas->structureSet || !glCanvas->structureSet->IsCDD()) return;
    switch (event.GetId()) {
        case MID_EDIT_CDD_DESCR: {
            wxString newDescription = wxGetTextFromUser("Enter or edit the CDD description text:",
                "CDD Description", glCanvas->structureSet->GetCDDDescription().c_str(),
                this, -1, -1, false);
            if (newDescription.size() > 0 &&
                !glCanvas->structureSet->SetCDDDescription(newDescription.c_str()))
                ERR_POST(Error << "Error saving CDD description");
            break;
        }
        case MID_EDIT_CDD_NOTES: {
            StructureSet::TextLines lines;
            if (!glCanvas->structureSet->GetCDDNotes(&lines)) break;
            MultiTextDialog dialog(lines,
                this, -1, "CDD Notes", wxDefaultPosition, wxSize(500, 400));
            int retval = dialog.ShowModal();
            if (retval == wxOK) {
                dialog.GetLines(&lines);
                if (!glCanvas->structureSet->SetCDDNotes(lines))
                    ERR_POST(Error << "Error saving CDD notes");
            }
            break;
        }
        case MID_ANNOT_CDD: {
            CDDAnnotateDialog dialog(this, glCanvas->structureSet);
            dialog.ShowModal();
            break;
        }
    }
}

void Cn3DMainFrame::OnAdjustView(wxCommandEvent& event)
{
    glCanvas->SetCurrent();
    switch (event.GetId()) {
        case MID_ZOOM_IN:       glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomIn); break;
        case MID_ZOOM_OUT:      glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomOut); break;
        case MID_RESET:         glCanvas->renderer->ResetCamera(); break;
        case MID_FIRST_FRAME:   glCanvas->renderer->ShowFirstFrame(); break;
        case MID_LAST_FRAME:    glCanvas->renderer->ShowLastFrame(); break;
        case MID_NEXT_FRAME:    glCanvas->renderer->ShowNextFrame(); break;
        case MID_PREV_FRAME:    glCanvas->renderer->ShowPreviousFrame(); break;
        case MID_ALL_FRAMES:    glCanvas->renderer->ShowAllFrames(); break;
        default:
            break;
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
            dialog.ShowModal();
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
        case MID_SHOW_UNALIGNED_ALL:
            glCanvas->structureSet->showHideManager->ShowResidues(glCanvas->structureSet, false);
            break;
        case MID_SHOW_UNALIGNED_ALN_DOMAIN:
            glCanvas->structureSet->showHideManager->
                ShowUnalignedResiduesInAlignedDomains(glCanvas->structureSet);
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

#define RENDERING_SHORTCUT(type) \
    glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::type);\
    break
#define COLORING_SHORTCUT(type) \
    glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::type);\
    break

void Cn3DMainFrame::OnSetStyle(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->SetCurrent();
        switch (event.GetId()) {
            case MID_EDIT_STYLE:
                if (!glCanvas->structureSet->styleManager->EditGlobalStyle(this))
                    return;
                break;
            case MID_ANNOTATE:
                if (!glCanvas->structureSet->styleManager->EditUserAnnotations(this))
                    return;
                break;
            case MID_WORM: RENDERING_SHORTCUT(eWormShortcut);
            case MID_TUBE: RENDERING_SHORTCUT(eTubeShortcut);
            case MID_WIRE: RENDERING_SHORTCUT(eWireframeShortcut);
            case MID_BNS: RENDERING_SHORTCUT(eBallAndStickShortcut);
            case MID_SPACE: RENDERING_SHORTCUT(eSpacefillShortcut);
            case MID_SC_TOGGLE: RENDERING_SHORTCUT(eToggleSidechainsShortcut);
            case MID_SECSTRUC: COLORING_SHORTCUT(eSecondaryStructureShortcut);
            case MID_ALIGNED: COLORING_SHORTCUT(eAlignedShortcut);
            case MID_IDENTITY: COLORING_SHORTCUT(eIdentityShortcut);
            case MID_VARIETY: COLORING_SHORTCUT(eVarietyShortcut);
            case MID_WGHT_VAR: COLORING_SHORTCUT(eWeightedVarietyShortcut);
            case MID_INFO: COLORING_SHORTCUT(eInformationContentShortcut);
            case MID_FIT: COLORING_SHORTCUT(eFitShortcut);
            case MID_OBJECT: COLORING_SHORTCUT(eObjectShortcut);
            case MID_DOMAIN: COLORING_SHORTCUT(eDomainShortcut);
            case MID_MOLECULE: COLORING_SHORTCUT(eMoleculeShortcut);
            case MID_HYDROPHOB: COLORING_SHORTCUT(eHydrophobicityShortcut);
            case MID_TEMP: COLORING_SHORTCUT(eTemperatureShortcut);
            case MID_ELEMENT: COLORING_SHORTCUT(eElementShortcut);
            default:
                return;
        }
        glCanvas->structureSet->styleManager->CheckGlobalStyleSettings();
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
        userDir = std::string(wxPathOnly(filename).c_str()) + wxFILE_SEP_PATH;
    else if (wxPathOnly(filename) == "")
        userDir = workingDir;
    else
        userDir = workingDir + wxPathOnly(filename).c_str() + wxFILE_SEP_PATH;
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
            glCanvas->structureSet = new StructureSet(cdd, userDir.c_str(), structureLimit);
        } else {
            ERR_POST(Warning << "error: " << err);
            delete cdd;
        }
    }
    if (!readOK) {
        ERR_POST(Error << "File is not a recognized data type (Ncbi-mime-asn1 or Cdd)");
        return;
    }

    SetTitle(wxString(wxFileNameFromPath(filename)) + " - Cn3D++");
    menuBar->EnableTop(menuBar->FindMenu("CDD"), glCanvas->structureSet->IsCDD());
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

    // force a save of any edits to alignment and updates first
    GlobalMessenger()->SequenceWindowsSave();

    wxString outputFilename = wxFileSelector(
        "Choose a filename for output", userDir.c_str(), "",
        ".prt", "All Files|*.*|Binary ASN (*.val)|*.val|ASCII CDD (*.acd)|*.acd|ASCII ASN (*.prt)|*.prt",
        wxSAVE | wxOVERWRITE_PROMPT);
    TESTMSG("save file: '" << outputFilename.c_str() << "'");
    if (!outputFilename.IsEmpty())
        glCanvas->structureSet->SaveASNData(outputFilename.c_str(), (outputFilename.Right(4) == ".val"));
}

void Cn3DMainFrame::OnLimit(wxCommandEvent& event)
{
    long newLimit = wxGetNumberFromUser(
        "Enter the maximum number of structures to display (-1 for unlimited)",
        "Max:", "Structure Limit", 10, -1, 1000, this);
    if (newLimit >= 0)
        structureLimit = (int) newLimit;
    else
        structureLimit = UNLIMITED_STRUCTURES;
}


// data and methods for the GLCanvas used for rendering structures (Cn3DGLCanvas)

BEGIN_EVENT_TABLE(Cn3DGLCanvas, wxGLCanvas)
    EVT_SIZE                (Cn3DGLCanvas::OnSize)
    EVT_PAINT               (Cn3DGLCanvas::OnPaint)
    EVT_MOUSE_EVENTS        (Cn3DGLCanvas::OnMouseEvent)
    EVT_ERASE_BACKGROUND    (Cn3DGLCanvas::OnEraseBackground)
END_EVENT_TABLE()

Cn3DGLCanvas::Cn3DGLCanvas(wxWindow *parent, int *attribList) :
    wxGLCanvas(parent, -1, wxPoint(0, 0), wxDefaultSize, wxSUNKEN_BORDER, "Cn3DGLCanvas", attribList),
    structureSet(NULL), font(NULL)
{
    renderer = new OpenGLRenderer();
}

Cn3DGLCanvas::~Cn3DGLCanvas(void)
{
    if (structureSet) delete structureSet;
    if (font) delete font;
    delete renderer;
}

void Cn3DGLCanvas::SetupFontFromRegistry(void)
{
    // delete old font
    if (font) delete font;

    // get font info from registry, and create wxFont
    int size, family, style, weight;
    bool underlined;
    if (!RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, &size) ||
        !RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_FAMILY, &family) ||
        !RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_STYLE, &style) ||
        !RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_WEIGHT, &weight) ||
        !RegistryGetBoolean(REG_OPENGL_FONT_SECTION, REG_FONT_UNDERLINED, &underlined) ||
        !(font = new wxFont(size, family, style, weight, underlined)))
    {
        ERR_POST(Error << "Cn3DGLCanvas::SetGLFont() - error setting up font");
        return;
    }

    // set up font display lists in renderer (needs "native" font structure)
    SetCurrent();
#if defined(__WXMSW__)
    renderer->SetFont_Windows(font->GetHFONT());
#elif defined(__WXGTK__)
    renderer->SetFont_GTK(font->GetInternalFont());
#endif
}

void Cn3DGLCanvas::OnPaint(wxPaintEvent& event)
{
    // This is a dummy, to avoid an endless succession of paint messages.
    // OnPaint handlers must always create a wxPaintDC.
    wxPaintDC dc(this);

    if (!GetContext() || !renderer) return;
    SetCurrent();
    renderer->Display();
    SwapBuffers();
}

void Cn3DGLCanvas::OnSize(wxSizeEvent& event)
{
    if (!GetContext() || !renderer) return;
    SetCurrent();
    int width, height;
    GetClientSize(&width, &height);
    renderer->SetSize(width, height);
}

void Cn3DGLCanvas::OnMouseEvent(wxMouseEvent& event)
{
    static bool dragging = false;
    static long last_x, last_y;

    if (!GetContext() || !renderer) return;
    SetCurrent();

    // keep mouse focus while holding down button
    if (event.LeftDown()) CaptureMouse();
    if (event.LeftUp()) ReleaseMouse();

    if (event.LeftIsDown()) {
        if (!dragging) {
            dragging = true;
        } else {
            OpenGLRenderer::eViewAdjust action;
            if (event.ShiftDown())
                action = OpenGLRenderer::eXYTranslateHV;    // shift-drag = translate
            else if (event.ControlDown())
                action = OpenGLRenderer::eZoomH;            // ctrl-drag = zoom
            else
                action = OpenGLRenderer::eXYRotateHV;       // normal rotate
            renderer->ChangeView(action, event.GetX() - last_x, event.GetY() - last_y);
            Refresh(false);
        }
        last_x = event.GetX();
        last_y = event.GetY();
    } else {
        dragging = false;
    }

    if (event.LeftDClick()) {   // double-click = select, +ctrl = set center
        unsigned int name;
        if (structureSet && renderer->GetSelected(event.GetX(), event.GetY(), &name))
            structureSet->SelectedAtom(name, event.ControlDown());
    }
}

void Cn3DGLCanvas::OnEraseBackground(wxEraseEvent& event)
{
    // Do nothing, to avoid flashing.
}


///// misc stuff /////

bool RegistryIsValidInteger(const std::string& section, const std::string& name)
{
    long value;
    wxString regStr = registry.Get(section, name).c_str();
    return (regStr.size() > 0 && regStr.ToLong(&value));
}

bool RegistryIsValidBoolean(const std::string& section, const std::string& name)
{
    std::string regStr = registry.Get(section, name);
    return (regStr.size() > 0 && (
        toupper(regStr[0]) == 'T' || toupper(regStr[0]) == 'F' ||
        toupper(regStr[0]) == 'Y' || toupper(regStr[0]) == 'N'));
}

bool RegistryGetInteger(const std::string& section, const std::string& name, int *value)
{
    wxString regStr = registry.Get(section, name).c_str();
    long l;
    if (regStr.size() == 0 || !regStr.ToLong(&l)) {
        ERR_POST(Warning << "Can't get long from registry: " << section << ", " << name);
        return false;
    }
    *value = (int) l;
    return true;
}

bool RegistryGetBoolean(const std::string& section, const std::string& name, bool *value)
{
    std::string regStr = registry.Get(section, name);
    if (regStr.size() == 0 || !(
            toupper(regStr[0]) == 'T' || toupper(regStr[0]) == 'F' ||
            toupper(regStr[0]) == 'Y' || toupper(regStr[0]) == 'N')) {
        ERR_POST(Warning << "Can't get boolean from registry: " << section << ", " << name);
        return false;
    }
    *value = (toupper(regStr[0]) == 'T' || toupper(regStr[0]) == 'Y');
    return true;
}

bool RegistrySetInteger(const std::string& section, const std::string& name, int value)
{
    wxString regStr;
    regStr.Printf("%i", value);
    bool okay = registry.Set(section, name, regStr.c_str(), CNcbiRegistry::ePersistent);
    if (!okay)
        ERR_POST(Error << "registry Set() failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetBoolean(const std::string& section, const std::string& name, bool value, bool useYesOrNo)
{
    std::string regStr;
    if (useYesOrNo)
        regStr = value ? "yes" : "no";
    else
        regStr = value ? "true" : "false";
    bool okay = registry.Set(section, name, regStr, CNcbiRegistry::ePersistent);
    if (!okay)
        ERR_POST(Error << "registry Set() failed");
    else
        registryChanged = true;
    return okay;
}

END_SCOPE(Cn3D)

