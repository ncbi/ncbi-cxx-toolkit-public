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
* Revision 1.120  2002/02/01 13:58:01  thiessen
* add hourglass cursor on file load
*
* Revision 1.119  2002/01/19 15:25:04  thiessen
* tweaks for DebugMT build to work
*
* Revision 1.118  2002/01/18 15:41:36  thiessen
* add Mac file type/creator to output files
*
* Revision 1.117  2002/01/18 13:55:29  thiessen
* add help menu and viewer
*
* Revision 1.116  2002/01/11 15:48:52  thiessen
* update for Mac CW7
*
* Revision 1.115  2002/01/03 16:18:40  thiessen
* add distance selection
*
* Revision 1.114  2001/12/21 14:13:02  thiessen
* tweak animation timer and menu stuff
*
* Revision 1.113  2001/12/21 13:52:20  thiessen
* add spin animation
*
* Revision 1.112  2001/12/20 21:41:45  thiessen
* create/use user preferences directory
*
* Revision 1.111  2001/12/15 03:15:58  thiessen
* adjustments for slightly changed object loader Set...() API
*
* Revision 1.110  2001/12/14 14:52:38  thiessen
* add apple event handler for open document
*
* Revision 1.109  2001/12/07 19:33:32  thiessen
* fix extra path sep. in save file name
*
* Revision 1.108  2001/12/06 23:13:44  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.107  2001/12/04 16:14:45  thiessen
* add frame icon
*
* Revision 1.106  2001/11/27 16:26:07  thiessen
* major update to data management system
*
* Revision 1.105  2001/11/09 15:19:19  thiessen
* wxFindFirst file fixed on wxMac; call glViewport() from OnSize()
*
* Revision 1.104  2001/11/01 19:01:42  thiessen
* use meta key instead of ctrl on Mac
*
* Revision 1.103  2001/10/30 02:54:12  thiessen
* add Biostruc cache
*
* Revision 1.102  2001/10/25 14:18:30  thiessen
* fix MeasureText problem (different red)
*
* Revision 1.101  2001/10/24 22:02:02  thiessen
* fix wxGTK concurrent rendering problem
*
* Revision 1.100  2001/10/23 20:10:23  thiessen
* fix scaling of fonts in high-res PNG output
*
* Revision 1.99  2001/10/23 13:53:37  thiessen
* add PNG export
*
* Revision 1.98  2001/10/20 20:16:32  thiessen
* don't use wxDefaultPosition for dialogs (win2000 problem)
*
* Revision 1.97  2001/10/17 17:46:22  thiessen
* save camera setup and rotation center in files
*
* Revision 1.96  2001/10/16 21:49:07  thiessen
* restructure MultiTextDialog; allow virtual bonds for alpha-only PDB's
*
* Revision 1.95  2001/10/13 14:13:25  thiessen
* fix wx version sting
*
* Revision 1.94  2001/10/12 14:52:05  thiessen
* fix for wxGTK crash
*
* Revision 1.93  2001/10/11 14:18:57  thiessen
* make MultiTextDialog non-modal
*
* Revision 1.92  2001/10/09 18:57:05  thiessen
* add CDD references editing dialog
*
* Revision 1.91  2001/10/08 14:18:33  thiessen
* fix show/hide dialog under wxGTK
*
* Revision 1.90  2001/10/08 00:00:08  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.89  2001/10/02 18:01:06  thiessen
* fix CDD annotate dialog bug
*
* Revision 1.88  2001/10/01 16:04:24  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.87  2001/09/27 15:37:58  thiessen
* decouple sequence import and BLAST
*
* Revision 1.86  2001/09/26 15:27:54  thiessen
* tweak sequence viewer widget for wx2.3.2, tweak cdd annotation
*
* Revision 1.85  2001/09/24 16:43:25  thiessen
* add wxGLApp test code
*
* Revision 1.84  2001/09/20 14:07:08  thiessen
* remove visual selection stuff
*
* Revision 1.83  2001/09/18 03:10:45  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.82  2001/09/14 14:56:24  thiessen
* increase default animation delay
*
* Revision 1.81  2001/09/14 14:46:22  thiessen
* working GL fonts
*
* Revision 1.80  2001/09/06 21:38:43  thiessen
* tweak message log / diagnostic system
*
* Revision 1.79  2001/09/06 18:34:28  thiessen
* more mac tweaks
*
* Revision 1.78  2001/09/06 18:15:44  thiessen
* fix OpenGL window initialization/OnSize to work on Mac
*
* Revision 1.77  2001/09/06 13:10:10  thiessen
* tweak show hide dialog layout
*
* Revision 1.76  2001/09/04 20:06:37  thiessen
* tweaks for Mac
*
* Revision 1.75  2001/09/04 14:40:19  thiessen
* add rainbow and charge coloring
*
* Revision 1.74  2001/09/02 11:26:02  thiessen
* add mac font defaults
*
* Revision 1.73  2001/08/31 22:24:40  thiessen
* add timer for animation
*
* Revision 1.72  2001/08/28 17:33:33  thiessen
* add ChooseGLVisual for wxWin 2.3+
*
* Revision 1.71  2001/08/24 18:53:42  thiessen
* add filename to sequence viewer window titles
*
* Revision 1.70  2001/08/24 13:29:27  thiessen
* header and GTK font tweaks
*
* Revision 1.69  2001/08/24 00:41:35  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.68  2001/08/16 19:21:02  thiessen
* add face name info to fonts
*
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

#ifdef __WXMSW__
#include <windows.h>
#ifdef Yield
#undef Yield
#endif
#ifdef DrawText
#undef DrawText
#endif
#ifdef CreateDialog
#undef CreateDialog
#endif
#endif

#include <wx/string.h> // kludge for now to fix weird namespace conflict

#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <ctools/ctools.h>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cn3d/Cn3d_style_settings_set.hpp>

#include <wx/wx.h>
#include <wx/image.h>
#include <wx/html/helpfrm.h>
#include <wx/html/helpctrl.h>
#include <wx/filesys.h>
#include <wx/fs_zip.h>
#include <wx/file.h>
#include <wx/fontdlg.h>
#include <wx/confbase.h>
#include <wx/fileconf.h>

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
#include "cn3d/cdd_annot_dialog.hpp"
#include "cn3d/preferences_dialog.hpp"
#include "cn3d/cdd_ref_dialog.hpp"
#include "cn3d/cn3d_png.hpp"
#include "cn3d/wx_tools.hpp"

#include <ncbienv.h>

#ifdef __WXMAC__
#include <wx/filename.h>
#include "MoreCarbonAccessors.h"
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// `Main program' equivalent, creating GUI framework
IMPLEMENT_APP(Cn3D::Cn3DApp)

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d/cn3d.xpm"
#endif


BEGIN_SCOPE(Cn3D)

// global strings for various directories - will include trailing path separator character
std::string
    workingDir,     // current working directory
    userDir,        // directory of latest user-selected file
    programDir,     // directory where Cn3D executable lives
    dataDir,        // 'data' directory with external data files
    currentFile,    // name of curruent working file
    prefsDir;       // application preferences directory
const std::string& GetWorkingDir(void) { return workingDir; }
const std::string& GetUserDir(void) { return userDir; }
const std::string& GetProgramDir(void) { return programDir; }
const std::string& GetDataDir(void) { return dataDir; }
const std::string& GetWorkingFilename(void) { return currentFile; }
const std::string& GetPrefsDir(void) { return prefsDir; }

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
static bool LoadFavorites(void);
static void SaveFavorites(void);
static void SetupFavoritesMenu(wxMenu *favoritesMenu);


// Set the NCBI diagnostic streams to go to this method, which then pastes them
// into a wxWindow. This log window can be closed anytime, but will be hidden,
// not destroyed. More serious errors will bring up a dialog, so that the user
// will get a chance to see the error before the program halts upon a fatal error.

class MsgFrame;

static MsgFrame *logFrame = NULL;
static std::list < std::string > backLog;

class MsgFrame : public wxFrame
{
public:
    wxTextCtrl *logText;
    int totalChars;
    MsgFrame(const wxString& title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) :
        wxFrame(GlobalTopWindow(), wxID_HIGHEST + 5, title, pos, size,
            wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT
#if wxVERSION_NUMBER >= 2302
                | wxFRAME_NO_TASKBAR
#endif
            ) { totalChars = 0; }
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
    std::string errMsg;
    diagMsg.Write(errMsg);
#ifdef __WXMAC__
    // wxTextCtrl on Mac doesn't like regular '\n' newlines... ugh
    if (errMsg[errMsg.size() - 1] < ' ') errMsg[errMsg.size() - 1] = '\r';
#endif

    // severe errors get a special error dialog
    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace) {
        wxMessageDialog dlg(NULL, errMsg.c_str(), "Severe Error!", wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg.ShowModal();
    }

    // info messages and less severe errors get added to the log
    else {
        if (logFrame) {
            // seems to be some upper limit on size, at least under MSW - so delete top of log if too big
            if (logFrame->totalChars + errMsg.size() > 32000) {
                logFrame->logText->Clear();
                logFrame->totalChars = 0;
            }
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
        logFrame = new MsgFrame("Cn3D++ Message Log", wxPoint(500, 0), wxSize(500, 500));
        logFrame->SetSizeHints(150, 100);
        logFrame->logText = new wxTextCtrl(logFrame, -1, "",
            wxPoint(0,0), wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
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


#ifdef TEST_WXGLAPP
BEGIN_EVENT_TABLE(Cn3DApp, wxGLApp)
#else
BEGIN_EVENT_TABLE(Cn3DApp, wxApp)
#endif
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

#ifdef TEST_WXGLAPP
Cn3DApp::Cn3DApp() : wxGLApp()
#else
Cn3DApp::Cn3DApp() : wxApp()
#endif
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info); // report all messages
    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams

#ifdef TEST_WXGLAPP
    if (!InitGLVisual(NULL)) ERR_POST(Fatal << "InitGLVisual failed");
#else
    // try to force all windows to use best (TrueColor) visuals
    SetUseBestVisual(true);
#endif
}

void Cn3DApp::InitRegistry(void)
{
    // first set up defaults, then override any/all with stuff from registry file

    // default animation delay and log window startup
    RegistrySetInteger(REG_CONFIG_SECTION, REG_ANIMATION_DELAY, 500);
    RegistrySetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, false);

    // default quality settings
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, 10);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, 8);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, 12);
    RegistrySetBoolean(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, true);

    // default font for OpenGL (structure window)
#if defined(__WXMSW__)
    RegistrySetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, 12);
#elif defined(__WXGTK__)
    RegistrySetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, 14);
#elif defined(__WXMAC__)
    RegistrySetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, 14);
#endif
    RegistrySetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_FAMILY, wxSWISS);
    RegistrySetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_STYLE, wxNORMAL);
    RegistrySetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_WEIGHT, wxBOLD);
    RegistrySetBoolean(REG_OPENGL_FONT_SECTION, REG_FONT_UNDERLINED, false);
    RegistrySetString(REG_OPENGL_FONT_SECTION, REG_FONT_FACENAME, FONT_FACENAME_UNKNOWN);

    // default font for sequence viewers
#if defined(__WXMSW__)
    RegistrySetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_SIZE, 10);
#elif defined(__WXGTK__)
    RegistrySetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_SIZE, 14);
#elif defined(__WXMAC__)
    RegistrySetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_SIZE, 12);
#endif
    RegistrySetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_FAMILY, wxROMAN);
    RegistrySetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_STYLE, wxNORMAL);
    RegistrySetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_WEIGHT, wxNORMAL);
    RegistrySetBoolean(REG_SEQUENCE_FONT_SECTION, REG_FONT_UNDERLINED, false);
    RegistrySetString(REG_SEQUENCE_FONT_SECTION, REG_FONT_FACENAME, FONT_FACENAME_UNKNOWN);

    // default cache settings
    RegistrySetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, true);
    if (GetPrefsDir().size() > 0)
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetPrefsDir() + "cache");
    else
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetProgramDir() + "cache");
    RegistrySetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, 25);

    // load program registry - overriding defaults if present
    if (GetPrefsDir().size() > 0)
        registryFile = GetPrefsDir() + "Preferences";
    else
        registryFile = GetProgramDir() + "Preferences";
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(registryFile.c_str(), IOS_BASE::in));
    if (*iniIn) {
        TESTMSG("loading program registry " << registryFile);
        registry.Read(*iniIn);
    }

    registryChanged = false;
}

bool Cn3DApp::OnInit(void)
{
    TESTMSG("Welcome to Cn3D++!\nbuilt " << __DATE__ << " with " << wxVERSION_STRING);

    // help system loads from zip file
#if wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB
    wxFileSystem::AddHandler(new wxZipFSHandler);
#else
#error Must turn on wxUSE_STREAMS && wxUSE_ZIPSTREAM && wxUSE_ZLIB for the Cn3D help system!
#endif

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
        ERR_POST(Warning << "Can't create Cn3D_User folder at either:"
            << "\n    " << prefsDirLocal
            << "\nor  " << prefsDirProg);
    else
        prefsDir += wxFILE_SEP_PATH;

    // set data dir, and register the path in C toolkit registry (mainly for BLAST code)
    dataDir = programDir + "data" + wxFILE_SEP_PATH;
    Nlm_TransientSetAppParam("ncbi", "ncbi", "data", dataDir.c_str());

    TESTMSG("working dir: " << workingDir.c_str());
    TESTMSG("program dir: " << programDir.c_str());
    TESTMSG("data dir: " << dataDir.c_str());
    TESTMSG("prefs dir: " << prefsDir.c_str());

    // read dictionary
    wxString dictFile = wxString(dataDir.c_str()) + "bstdt.val";
    LoadStandardDictionary(dictFile.c_str());

    // set up registry and favorite styles (must be done before structure window creation)
    InitRegistry();
    LoadFavorites();

    // create the main frame window - must be first window created by the app
    structureWindow = new Cn3DMainFrame("Cn3D++", wxPoint(0,0), wxSize(500,500));
    SetTopWindow(structureWindow);
    topWindow = structureWindow;

    // show log if set to do so
    bool showLog = false;
    RegistryGetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, &showLog);
#ifndef __WXMAC__
    if (showLog) RaiseLogWindow();
#endif

    // get file name from command line, if present
    if (argc == 2)
        structureWindow->LoadFile(argv[1]);
    else {
        if (argc > 2) ERR_POST(Error << "\nUsage: cn3d [filename]");
        structureWindow->glCanvas->renderer->AttachStructureSet(NULL);
    }

    // give structure window initial focus
    structureWindow->Raise();
    structureWindow->SetFocus();
#ifdef __WXMAC__
    if (showLog) RaiseLogWindow();
#endif
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
//            TESTMSG("saving program registry " << registryFile);
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

#ifdef __WXMAC__
// borrowed from vibwndws.c
static void ConvertFilename ( FSSpec *fss, char *filename )
{
    register char *src;
    register char *dst;
    register int i;
    char buffer [256];

    Str255 dirname;
    DirInfo dinfo;

    src = buffer;
    dinfo.ioDrParID = fss->parID;
    dinfo.ioNamePtr = dirname;
    do {
        dinfo.ioVRefNum = fss->vRefNum;
        dinfo.ioFDirIndex = -1;
        dinfo.ioDrDirID = dinfo.ioDrParID;
        PBGetCatInfo ((CInfoPBPtr) &dinfo, 0);

        *src++ = ':';
        for ( i=dirname[0]; i; i-- )
        *src++ = dirname [i];
    } while ( dinfo.ioDrDirID != 2 );

    /* Reverse the file path! */
    dst = filename;
    while ( src != buffer )
    *dst++ = *(--src);
    for( i = 1; i <= fss->name [0]; i++ )
    *dst++ = fss->name [i];
    *dst = '\0';
}

// special handler for open file apple event
short Cn3DApp::MacHandleAEODoc(const WXAPPLEEVENTREF event, WXAPPLEEVENTREF reply)
{
    // borrowed from HandleAEOpenDoc() in vibwndws.c
    register OSErr stat;
    register long i;
    AEDescList list;
    AEKeyword keywd;
    DescType dtype;
    FSSpec fss;
    long count;
    Size size;
    char filename [256];

    stat = AEGetParamDesc ((const AEDesc *) event, keyDirectObject, typeAEList, &list);
    if ( stat ) return ( stat );

    stat = AEGetAttributePtr ((const AEDesc *) event, keyMissedKeywordAttr, typeWildCard, &dtype, 0, 0, &size );
    if ( stat != errAEDescNotFound ) {
        AEDisposeDesc( &list );
        return ( stat? stat : errAEEventNotHandled );
    }

    // try to extract a file name to open
    *filename = '\0';
    AECountItems ( &list, &count );
    for ( i = 1; i <= count; i++ ) {
        stat = AEGetNthPtr (&list, i, typeFSS, &keywd, &dtype, (Ptr) &fss, sizeof (fss), &size);
        if ( !stat ) {
            ConvertFilename (&fss, filename);
            break;
        }
    }

    // actually open the file
    if (*filename) structureWindow->LoadFile(filename);

    // borrowed from wxApp::MacHandleAEODoc
    ProcessSerialNumber PSN ;
    PSN.highLongOfPSN = 0 ;
    PSN.lowLongOfPSN = kCurrentProcess ;
    SetFrontProcess( &PSN ) ;
    return noErr ;
}
#endif


static wxString GetFavoritesFile(bool forRead)
{
    // try to get value from registry
    wxString file = registry.Get(REG_CONFIG_SECTION, REG_FAVORITES_NAME).c_str();

    // if not set, ask user for a folder, then set in registry
    if (file.size() == 0) {
        file = wxFileSelector("Select a file for favorites:",
            prefsDir.c_str(), "Favorites", "", "*.*",
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
            if (ReadASNFromFile(favoritesFile.c_str(), &favoriteStyles, false, &err)) {
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
                if (!WriteASNToFile(favoritesFile.c_str(), favoriteStyles, false, &err))
                    ERR_POST(Error << "Error saving Favorites to " << favoritesFile << '\n' << err);
                favoriteStylesChanged = false;
            }
        }
    }
}

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


// data and methods for the main program window (a Cn3DMainFrame)

BEGIN_EVENT_TABLE(Cn3DMainFrame, wxFrame)
    EVT_CLOSE     (                                         Cn3DMainFrame::OnCloseWindow)
    EVT_MENU      (MID_EXIT,                                Cn3DMainFrame::OnExit)
    EVT_MENU      (MID_OPEN,                                Cn3DMainFrame::OnOpen)
    EVT_MENU      (MID_SAVE,                                Cn3DMainFrame::OnSave)
    EVT_MENU      (MID_PNG,                                 Cn3DMainFrame::OnPNG)
    EVT_MENU_RANGE(MID_ZOOM_IN,  MID_ALL_FRAMES,            Cn3DMainFrame::OnAdjustView)
    EVT_MENU_RANGE(MID_SHOW_HIDE,  MID_SHOW_SELECTED,       Cn3DMainFrame::OnShowHide)
    EVT_MENU_RANGE(MID_DIST_SELECT_RESIDUES, MID_DIST_SELECT_ALL, Cn3DMainFrame::OnDistanceSelect)
    EVT_MENU      (MID_REFIT_ALL,                           Cn3DMainFrame::OnAlignStructures)
    EVT_MENU_RANGE(MID_EDIT_STYLE, MID_ANNOTATE,            Cn3DMainFrame::OnSetStyle)
    EVT_MENU_RANGE(MID_ADD_FAVORITE, MID_FAVORITES_FILE,    Cn3DMainFrame::OnEditFavorite)
    EVT_MENU_RANGE(MID_FAVORITES_BEGIN, MID_FAVORITES_END,  Cn3DMainFrame::OnSelectFavorite)
    EVT_MENU_RANGE(MID_SHOW_LOG,   MID_SHOW_SEQ_V,          Cn3DMainFrame::OnShowWindow)
    EVT_MENU_RANGE(MID_EDIT_CDD_NAME, MID_ANNOT_CDD,        Cn3DMainFrame::OnCDD)
    EVT_MENU      (MID_PREFERENCES,                         Cn3DMainFrame::OnPreferences)
    EVT_MENU_RANGE(MID_OPENGL_FONT, MID_SEQUENCE_FONT,      Cn3DMainFrame::OnSetFont)
    EVT_MENU      (MID_LIMIT_STRUCT,                        Cn3DMainFrame::OnLimit)
    EVT_MENU_RANGE(MID_PLAY, MID_SET_DELAY,                 Cn3DMainFrame::OnAnimate)
    EVT_TIMER     (MID_ANIMATE,                             Cn3DMainFrame::OnTimer)
    EVT_MENU      (MID_HELP_COMMANDS,                       Cn3DMainFrame::OnHelp)
END_EVENT_TABLE()

Cn3DMainFrame::Cn3DMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
    wxFrame(NULL, wxID_HIGHEST + 1, title, pos, size, wxDEFAULT_FRAME_STYLE | wxTHICK_FRAME),
    glCanvas(NULL), structureLimit(1000),
    cddAnnotateDialog(NULL), cddDescriptionDialog(NULL), cddNotesDialog(NULL),
    helpController(NULL), helpConfig(NULL)
{
    topWindow = this;
    GlobalMessenger()->AddStructureWindow(this);
    timer.SetOwner(this, MID_ANIMATE);
    SetSizeHints(150, 150); // min size
    SetIcon(wxICON(cn3d));

    // File menu
    menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_OPEN, "&Open");
    menu->Append(MID_SAVE, "&Save");
    menu->Append(MID_PNG, "&Export PNG");
    menu->AppendSeparator();
    menu->Append(MID_REFIT_ALL, "&Realign Structures");
    menu->Append(MID_LIMIT_STRUCT, "&Limit Structures");
    menu->AppendSeparator();
    menu->Append(MID_PREFERENCES, "&Preferences...");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_OPENGL_FONT, "S&tructure Window");
    subMenu->Append(MID_SEQUENCE_FONT, "Se&quence Windows");
    menu->Append(MID_FONTS, "Set &Fonts...", subMenu);
    menu->AppendSeparator();
    menu->Append(MID_EXIT, "E&xit");
    menuBar->Append(menu, "&File");

    // View menu
    menu = new wxMenu;
    menu->Append(MID_ZOOM_IN, "Zoom &In\tz");
    menu->Append(MID_ZOOM_OUT, "Zoom &Out\tx");
    menu->Append(MID_RESTORE, "&Restore");
    menu->Append(MID_RESET, "Rese&t");
    menu->AppendSeparator();
#ifdef __WXMSW__
    menu->Append(MID_NEXT_FRAME, "&Next Frame\tRight");
    menu->Append(MID_PREV_FRAME, "Pre&vious Frame\tLeft");
    menu->Append(MID_FIRST_FRAME, "&First Frame\tDown");
    menu->Append(MID_LAST_FRAME, "&Last Frame\tUp");
#else
    // other platforms don't like to display these long accelerator names
    menu->Append(MID_NEXT_FRAME, "&Next Frame");
    menu->Append(MID_PREV_FRAME, "Pre&vious Frame");
    menu->Append(MID_FIRST_FRAME, "&First Frame");
    menu->Append(MID_LAST_FRAME, "&Last Frame");
#endif
    menu->Append(MID_ALL_FRAMES, "&All Frames\ta");
    menu->AppendSeparator();
    subMenu = new wxMenu;
    subMenu->Append(MID_PLAY, "&Play Frames\tp", "", true);
    subMenu->Check(MID_PLAY, false);
    subMenu->Append(MID_SPIN, "Spi&n\tn", "", true);
    subMenu->Check(MID_SPIN, false);
    subMenu->Append(MID_STOP, "&Stop\ts", "", true);
    subMenu->Check(MID_STOP, true);
    subMenu->Append(MID_SET_DELAY, "Set &Delay");
    menu->Append(MID_ANIMATE, "Ani&mation", subMenu);
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
    menu->AppendSeparator();
    subMenu = new wxMenu;
    subMenu->Append(MID_DIST_SELECT_RESIDUES, "&Residues Only");
    subMenu->Append(MID_DIST_SELECT_ALL, "&All Molecules");
    menu->Append(MID_DIST_SELECT, "Select by &Distance...", subMenu);
    menuBar->Append(menu, "Show/&Hide");

    // Style menu
    menu = new wxMenu;
    menu->Append(MID_EDIT_STYLE, "Edit &Global Style");
    // favorites
    favoritesMenu = new wxMenu;
    favoritesMenu->Append(MID_ADD_FAVORITE, "&Add/Replace");
    favoritesMenu->Append(MID_REMOVE_FAVORITE, "&Remove");
    favoritesMenu->Append(MID_FAVORITES_FILE, "&Change File");
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
    subMenu->Append(MID_RAINBOW, "&Rainbow");
    subMenu->Append(MID_HYDROPHOB, "&Hydrophobicity");
    subMenu->Append(MID_CHARGE, "&Charge");
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
    menu->Append(MID_SHOW_LOG_START, "Show Log on Start&up", "", true);
    bool showLog = false;
    RegistryGetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, &showLog);
    menu->Check(MID_SHOW_LOG_START, showLog);
    menuBar->Append(menu, "&Window");

    // CDD menu
    menu = new wxMenu;
    menu->Append(MID_EDIT_CDD_NAME, "Edit &Name");
    menu->Append(MID_EDIT_CDD_DESCR, "Edit &Description");
    menu->Append(MID_EDIT_CDD_NOTES, "Edit N&otes");
    menu->Append(MID_EDIT_CDD_REFERENCES, "Edit &References");
    menu->Append(MID_ANNOT_CDD, "&Annotate");
    menuBar->Append(menu, "&CDD");

    // Help menu
    menu = new wxMenu;
    menu->Append(MID_HELP_COMMANDS, "&Commands");
    menuBar->Append(menu, "&Help");

    // accelerators for special keys
    wxAcceleratorEntry entries[10];
    entries[0].Set(wxACCEL_NORMAL, WXK_RIGHT, MID_NEXT_FRAME);
    entries[1].Set(wxACCEL_NORMAL, WXK_LEFT, MID_PREV_FRAME);
    entries[2].Set(wxACCEL_NORMAL, WXK_DOWN, MID_FIRST_FRAME);
    entries[3].Set(wxACCEL_NORMAL, WXK_UP, MID_LAST_FRAME);
    entries[4].Set(wxACCEL_NORMAL, 'z', MID_ZOOM_IN);
    entries[5].Set(wxACCEL_NORMAL, 'x', MID_ZOOM_OUT);
    entries[6].Set(wxACCEL_NORMAL, 'a', MID_ALL_FRAMES);
    entries[7].Set(wxACCEL_NORMAL, 'p', MID_PLAY);
    entries[8].Set(wxACCEL_NORMAL, 'n', MID_SPIN);
    entries[9].Set(wxACCEL_NORMAL, 's', MID_STOP);
    wxAcceleratorTable accel(10, entries);
    SetAcceleratorTable(accel);

    SetMenuBar(menuBar);
    menuBar->EnableTop(menuBar->FindMenu("CDD"), false);

    // Make a GLCanvas
#if defined(__WXMSW__)
    int *attribList = NULL;
#elif defined(__WXGTK__)
    int *attribList = NULL;
#elif defined(__WXMAC__)
    int *attribList = NULL;
#endif
    glCanvas = new Cn3DGLCanvas(this, attribList);

    // set initial font
    Show(true); // on X, need to establish gl context first, which requires visible window
    glCanvas->SetCurrent();
    glCanvas->SetGLFontFromRegistry();
}

Cn3DMainFrame::~Cn3DMainFrame(void)
{
    if (logFrame) {
        logFrame->Destroy();
        logFrame = NULL;
    }
}

void Cn3DMainFrame::OnCloseWindow(wxCloseEvent& event)
{
    timer.Stop();
    Command(MID_EXIT);
}

void Cn3DMainFrame::OnExit(wxCommandEvent& event)
{
    timer.Stop();
    GlobalMessenger()->RemoveStructureWindow(this); // don't bother with any redraws since we're exiting
    GlobalMessenger()->SequenceWindowsSave();       // save any edited alignment and updates first
    SaveDialog(false);                              // give structure window a chance to save data
    SaveFavorites();

    // remove help window if present
    if (helpController) {
        if (helpController->GetFrame())
            helpController->GetFrame()->Close(true);
        wxConfig::Set(NULL);
        delete helpConfig; // saves data
        delete helpController;
    }

    DestroyNonModalDialogs();
    Destroy();
}

void Cn3DMainFrame::OnHelp(wxCommandEvent& event)
{
    if (event.GetId() == MID_HELP_COMMANDS) {
        if (!helpController) {
            wxString path = wxString(GetPrefsDir().c_str()) + "help_cache";
            if (!wxDirExists(path)) {
                TESTMSG("trying to create help cache folder " << path.c_str());
                wxMkdir(path);
            }
            helpController = new wxHtmlHelpController(wxHF_DEFAULTSTYLE);
            helpController->SetTempDir(path);
            path = path + wxFILE_SEP_PATH + "Config";
            TESTMSG("saving help config in " << path.c_str());
            helpConfig = new wxFileConfig("Cn3D", "NCBI", path);
            wxConfig::Set(helpConfig);
            helpController->UseConfig(wxConfig::Get());
            path = wxString(GetProgramDir().c_str()) + "cn3d_commands.htb";
            if (!helpController->AddBook(path))
                ERR_POST(Error << "Can't load help book at " << path.c_str());
        }
        if (event.GetId() == MID_HELP_COMMANDS)
            helpController->Display("Cn3D Commands");
    }
}

void Cn3DMainFrame::OnDistanceSelect(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;
    static double latestCutoff = 5.0;
    GetFloatingPointDialog dialog(this, "Enter a distance cutoff (in Angstroms):", "Distance?",
        0.0, 1000.0, 0.5, latestCutoff);
    if (dialog.ShowModal() == wxOK) {
        latestCutoff = dialog.GetValue();
        glCanvas->structureSet->SelectByDistance(latestCutoff, (event.GetId() == MID_DIST_SELECT_RESIDUES));
    }
}

void Cn3DMainFrame::OnPNG(wxCommandEvent& event)
{
    ExportPNG(glCanvas);
}

void Cn3DMainFrame::OnAnimate(wxCommandEvent& event)
{
    if (event.GetId() != MID_SET_DELAY) {
        menuBar->Check(MID_PLAY, false);
        menuBar->Check(MID_SPIN, false);
        menuBar->Check(MID_STOP, true);
    }
    if (!glCanvas->structureSet) return;

    int currentDelay;
    if (!RegistryGetInteger(REG_CONFIG_SECTION, REG_ANIMATION_DELAY, &currentDelay))
        return;

    // play
    if (event.GetId() == MID_PLAY) {
        if (glCanvas->structureSet->frameMap.size() > 1) {
            timer.Start(currentDelay, false);
            animationMode = ANIM_FRAMES;
            menuBar->Check(MID_PLAY, true);
            menuBar->Check(MID_STOP, false);
        }
    }

    // spin
    if (event.GetId() == MID_SPIN) {
        timer.Start(20, false);
        animationMode = ANIM_SPIN;
        menuBar->Check(MID_SPIN, true);
        menuBar->Check(MID_STOP, false);
    }

    // stop
    else if (event.GetId() == MID_STOP) {
        timer.Stop();
    }

    // set delay
    else if (event.GetId() == MID_SET_DELAY) {
        long newDelay = wxGetNumberFromUser("Enter a delay value in milliseconds (1..5000)",
            "Delay:", "Set Delay", (long) currentDelay, 1, 5000, this);
        if (newDelay > 0) {
            if (!RegistrySetInteger(REG_CONFIG_SECTION, REG_ANIMATION_DELAY, (int) newDelay))
                return;
            // change delay if timer is running
            if (timer.IsRunning()) {
                timer.Stop();
                timer.Start((int) newDelay, false);
            }
        }
    }
}

void Cn3DMainFrame::OnTimer(wxTimerEvent& event)
{
    if (animationMode == ANIM_FRAMES) {
        // simply pretend the user selected "next frame"
        Command(MID_NEXT_FRAME);
    }

    else if (animationMode == ANIM_SPIN) {
        // pretend the user dragged the mouse to the right
        glCanvas->renderer->ChangeView(OpenGLRenderer::eXYRotateHV, 4, 0);
        glCanvas->Refresh(false);
    }
}

void Cn3DMainFrame::OnSetFont(wxCommandEvent& event)
{
    std::string section, faceName;
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
        !RegistryGetBoolean(section, REG_FONT_UNDERLINED, &underlined) ||
        !RegistryGetString(section, REG_FONT_FACENAME, &faceName))
    {
        ERR_POST(Error << "Cn3DMainFrame::OnSetFont() - error setting up initial font");
        return;
    }
    wxFont initialFont(size, family, style, weight, underlined,
        (faceName == FONT_FACENAME_UNKNOWN) ? "" : faceName.c_str());
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
            !RegistrySetBoolean(section, REG_FONT_UNDERLINED, font.GetUnderlined(), true) ||
            !RegistrySetString(section, REG_FONT_FACENAME,
                (font.GetFaceName().size() > 0) ? font.GetFaceName().c_str() : FONT_FACENAME_UNKNOWN.c_str()))
        {
            ERR_POST(Error << "Cn3DMainFrame::OnSetFont() - error setting registry data");
            return;
        }

        // call font setup
        TESTMSG("setting new font");
        if (event.GetId() == MID_OPENGL_FONT) {
            glCanvas->SetCurrent();
            glCanvas->SetGLFontFromRegistry();
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
        case MID_SHOW_LOG_START:
            RegistrySetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START,
                menuBar->IsChecked(MID_SHOW_LOG_START));
            break;
    }
}

void Cn3DMainFrame::DialogTextChanged(const MultiTextDialog *changed)
{
    if (!changed || !glCanvas->structureSet) return;

    if (changed == cddNotesDialog) {
        StructureSet::TextLines lines;
        cddNotesDialog->GetLines(&lines);
        if (!glCanvas->structureSet->SetCDDNotes(lines))
            ERR_POST(Error << "Error saving CDD notes");
    }
    if (changed == cddDescriptionDialog) {
        string line;
        cddDescriptionDialog->GetLine(&line);
        if (!glCanvas->structureSet->SetCDDDescription(line))
            ERR_POST(Error << "Error saving CDD description");
    }
}

void Cn3DMainFrame::DialogDestroyed(const MultiTextDialog *destroyed)
{
    TESTMSG("destroying MultiTextDialog");
    if (destroyed == cddNotesDialog) cddNotesDialog = NULL;
    if (destroyed == cddDescriptionDialog) cddDescriptionDialog = NULL;
}

void Cn3DMainFrame::DestroyNonModalDialogs(void)
{
    if (cddAnnotateDialog) cddAnnotateDialog->Destroy();
    if (cddNotesDialog) cddNotesDialog->Destroy();
    if (cddDescriptionDialog) cddDescriptionDialog->Destroy();
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
        case MID_EDIT_CDD_NAME: {
            wxString newName = wxGetTextFromUser("Enter or edit the CDD name:",
                "CDD Name", glCanvas->structureSet->GetCDDName().c_str(), this, -1, -1, false);
            if (newName.size() > 0 && !glCanvas->structureSet->SetCDDName(newName.c_str()))
                ERR_POST(Error << "Error saving CDD name");
            break;
        }
        case MID_EDIT_CDD_DESCR:
            if (!cddDescriptionDialog) {
                StructureSet::TextLines line(1);
                line[0] = glCanvas->structureSet->GetCDDDescription().c_str();
                cddDescriptionDialog = new MultiTextDialog(this, line,
                    this, -1, "CDD Description", wxPoint(50, 50), wxSize(400, 200));
            }
            cddDescriptionDialog->Show(true);
            break;
        case MID_EDIT_CDD_NOTES:
            if (!cddNotesDialog) {
                StructureSet::TextLines lines;
                if (!glCanvas->structureSet->GetCDDNotes(&lines)) break;
                cddNotesDialog = new MultiTextDialog(this, lines,
                    this, -1, "CDD Notes", wxPoint(100, 100), wxSize(500, 400));
            }
            cddNotesDialog->Show(true);
            break;
        case MID_EDIT_CDD_REFERENCES: {
            CDDRefDialog dialog(glCanvas->structureSet, this, -1, "CDD References");
            dialog.ShowModal();
            break;
        }
        case MID_ANNOT_CDD: {
            if (!cddAnnotateDialog)
                cddAnnotateDialog = new CDDAnnotateDialog(this, &cddAnnotateDialog, glCanvas->structureSet);
            cddAnnotateDialog->Show(true);
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
        case MID_RESTORE:       glCanvas->renderer->RestoreSavedView(); break;
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
                titles, &structureVisibilities, glCanvas->structureSet->showHideManager, false,
                this, -1, "Show/Hide Structures", wxPoint(200, 50));
            dialog.ShowModal();
            //delete titles;    // apparently deleted by wxWindows
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
            case MID_RAINBOW: COLORING_SHORTCUT(eRainbowShortcut);
            case MID_HYDROPHOB: COLORING_SHORTCUT(eHydrophobicityShortcut);
            case MID_CHARGE: COLORING_SHORTCUT(eChargeShortcut);
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
    SetCursor(*wxHOURGLASS_CURSOR);
    glCanvas->SetCurrent();

    // clear old data
    if (glCanvas->structureSet) {
        DestroyNonModalDialogs();
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
        SetCursor(wxNullCursor);
        return;
    }
    currentFile = wxFileNameFromPath(filename);

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
        readOK = ReadASNFromFile(filename, mime, isBinary, &err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(mime, structureLimit, glCanvas->renderer);
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
        readOK = ReadASNFromFile(filename, cdd, isBinary, &err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(cdd, structureLimit, glCanvas->renderer);
        } else {
            ERR_POST(Warning << "error: " << err);
            delete cdd;
        }
    }
    if (!readOK) {
        ERR_POST(Error << "File is not a recognized data type (Ncbi-mime-asn1 or Cdd)");
        SetCursor(wxNullCursor);
        return;
    }

    SetTitle(wxString(currentFile.c_str()) + " - Cn3D++");
    menuBar->EnableTop(menuBar->FindMenu("CDD"), glCanvas->structureSet->IsCDD());
    glCanvas->structureSet->SetCenter();
    glCanvas->renderer->AttachStructureSet(glCanvas->structureSet);
    glCanvas->Refresh(false);
    SetCursor(wxNullCursor);
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

    wxString outputFolder = wxString(userDir.c_str(), userDir.size() - 1); // remove trailing /
    wxString outputFilename = wxFileSelector(
        "Choose a filename for output", outputFolder, "",
        ".prt", "All Files|*.*|Binary ASN (*.val)|*.val|ASCII CDD (*.acd)|*.acd|ASCII ASN (*.prt)|*.prt",
        wxSAVE | wxOVERWRITE_PROMPT);
    TESTMSG("save file: '" << outputFilename.c_str() << "'");

    if (!outputFilename.IsEmpty()) {
        glCanvas->structureSet->SaveASNData(outputFilename.c_str(), (outputFilename.Right(4) == ".val"));

#ifdef __WXMAC__
        // set mac file type and creator
        wxFileName wxfn(outputFilename);
        if (wxfn.FileExists())
            if (!wxfn.MacSetTypeAndCreator('TEXT', 'Cn3D'))
                ERR_POST(Warning << "Unable to set Mac file type/creator");
#endif

        // set path/name/title
        if (wxIsAbsolutePath(outputFilename))
            userDir = std::string(wxPathOnly(outputFilename).c_str()) + wxFILE_SEP_PATH;
        else if (wxPathOnly(outputFilename) == "")
            userDir = workingDir;
        else
            userDir = workingDir + wxPathOnly(outputFilename).c_str() + wxFILE_SEP_PATH;
        currentFile = wxFileNameFromPath(outputFilename);
        SetTitle(wxString(currentFile.c_str()) + " - Cn3D++");
        GlobalMessenger()->SetSequenceViewerTitles(currentFile);
    }
}

void Cn3DMainFrame::OnLimit(wxCommandEvent& event)
{
    long newLimit = wxGetNumberFromUser("Enter the maximum number of structures to display",
        "Max: ", "Structure Limit", structureLimit, 0, 1000, this);
    if (newLimit >= 0)
        structureLimit = (int) newLimit;
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
    structureSet(NULL), suspended(false)
{
    renderer = new OpenGLRenderer(this);
}

Cn3DGLCanvas::~Cn3DGLCanvas(void)
{
    if (structureSet) delete structureSet;
    delete renderer;
}

void Cn3DGLCanvas::SuspendRendering(bool suspend)
{
    suspended = suspend;
    if (!suspend) {
        wxSizeEvent resize(GetSize());
        OnSize(resize);
    }
}

void Cn3DGLCanvas::SetGLFontFromRegistry(double fontScale)
{
    // get font info from registry, and create wxFont
    int size, family, style, weight;
    bool underlined;
    std::string faceName;
    if (!RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_SIZE, &size) ||
        !RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_FAMILY, &family) ||
        !RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_STYLE, &style) ||
        !RegistryGetInteger(REG_OPENGL_FONT_SECTION, REG_FONT_WEIGHT, &weight) ||
        !RegistryGetBoolean(REG_OPENGL_FONT_SECTION, REG_FONT_UNDERLINED, &underlined) ||
        !RegistryGetString(REG_OPENGL_FONT_SECTION, REG_FONT_FACENAME, &faceName))
    {
        ERR_POST(Error << "Cn3DGLCanvas::SetGLFont() - error getting font info from registry");
        return;
    }

    // create new font - assignment uses object reference to copy
    if (fontScale != 1.0 && fontScale > 0.0) size *= fontScale;
    wxFont newFont(size, family, style, weight, underlined,
            (faceName == FONT_FACENAME_UNKNOWN) ? "" : faceName.c_str());
    font = newFont;

    // set up font display lists in dc and renderer
    if (!memoryDC.Ok()) {
        wxBitmap tinyBitmap(1, 1, -1);
        memoryBitmap = tinyBitmap; // copies by reference
        memoryDC.SelectObject(memoryBitmap);
    }
    memoryDC.SetFont(font);

    renderer->SetGLFont(0, 256, renderer->FONT_BASE);
}

#define MYMAX(a, b) (((a) >= (b)) ? (a) : (b))

bool Cn3DGLCanvas::MeasureText(const std::string& text, int *width, int *height, int *centerX, int *centerY)
{
    wxCoord w, h;
    memoryDC.GetTextExtent(text.c_str(), &w, &h);
    *width = (int) w;
    *height = (int) h;

    // GetTextExtent measures text+background when a character is drawn, but for OpenGL, we need
    // to measure actual character pixels more precisely: render characters into memory bitmap,
    // then find the minimal rect that contains actual character pixels, not text background
    if (memoryBitmap.GetWidth() < w || memoryBitmap.GetHeight() < h) {
        wxBitmap biggerBitmap(MYMAX(memoryBitmap.GetWidth(), w), MYMAX(memoryBitmap.GetHeight(), h), -1);
        memoryBitmap = biggerBitmap; // copies by reference
        memoryDC.SelectObject(memoryBitmap);
    }
    memoryDC.SetBackground(*wxBLUE_BRUSH);
    memoryDC.SetBackgroundMode(wxSOLID);
    memoryDC.SetTextBackground(*wxGREEN);
    memoryDC.SetTextForeground(*wxRED);

    memoryDC.BeginDrawing();
    memoryDC.Clear();
    memoryDC.DrawText(text.c_str(), 0, 0);
    memoryDC.EndDrawing();

    // then convert bitmap to image so that we can read individual pixels (ugh...)
    wxImage image(memoryBitmap);
//    wxInitAllImageHandlers();
//    image.SaveFile("text.png", wxBITMAP_TYPE_PNG); // for testing

    // now find extent of actual (red) text pixels; wx coords put (0,0) at upper left
    int x, y, top = image.GetHeight(), left = image.GetWidth(), bottom = -1, right = -1;
    for (x=0; x<image.GetWidth(); x++) {
        for (y=0; y<image.GetHeight(); y++) {
            if (image.GetRed(x, y) >= 128) { // character pixel here
                if (y < top) top = y;
                if (x < left) left = x;
                if (y > bottom) bottom = y;
                if (x > right) right = x;
            }
        }
    }
    if (bottom < 0 || right < 0) {
        ERR_POST(Warning << "Cn3DGLCanvas::MeasureText() - no character pixels found!");
        *centerX = *centerY = 0;
        return false;
    }
//    TESTMSG("top: " << top << ", left: " << left << ", bottom: " << bottom << ", right: " << right);

    // set center{X,Y} to center of drawn glyph relative to bottom left
    *centerX = (int) (((double) (right - left)) / 2 + 0.5);
    *centerY = (int) (((double) (bottom - top)) / 2 + 0.5);

    return true;
}

void Cn3DGLCanvas::OnPaint(wxPaintEvent& event)
{
    // This is a dummy, to avoid an endless succession of paint messages.
    // OnPaint handlers must always create a wxPaintDC.
    wxPaintDC dc(this);

    if (!GetContext() || !renderer || suspended) return;
    SetCurrent();
    renderer->Display();
    SwapBuffers();
}

void Cn3DGLCanvas::OnSize(wxSizeEvent& event)
{
    if (suspended || !GetContext()) return;
    SetCurrent();

    // this is necessary to update the context on some platforms
    wxGLCanvas::OnSize(event);

    // set GL viewport (not called by wxGLCanvas::OnSize on all platforms...)
	int w, h;
	GetClientSize(&w, &h);
    glViewport(0, 0, (GLint) w, (GLint) h);

    renderer->NewView();
}

void Cn3DGLCanvas::OnMouseEvent(wxMouseEvent& event)
{
    static bool dragging = false;
    static long last_x, last_y;

    if (!GetContext() || !renderer || suspended) return;
    SetCurrent();

// in wxGTK >= 2.3.2, this causes a system crash on some Solaris machines...
#if !defined(__WXGTK__) || wxVERSION_NUMBER < 2302
    // keep mouse focus while holding down button
    if (event.LeftDown()) CaptureMouse();
    if (event.LeftUp()) ReleaseMouse();
#endif

    if (event.LeftIsDown()) {
        if (!dragging) {
            dragging = true;
        } else {
            OpenGLRenderer::eViewAdjust action;
            if (event.ShiftDown())
                action = OpenGLRenderer::eXYTranslateHV;    // shift-drag = translate
#ifdef __WXMAC__
            else if (event.MetaDown())      // control key + mouse doesn't work on Mac?
#else
            else if (event.ControlDown())
#endif
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
            structureSet->SelectedAtom(name,
#ifdef __WXMAC__
                event.MetaDown()      // control key + mouse doesn't work on Mac?
#else
                event.ControlDown()
#endif
            );
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

bool RegistryIsValidString(const std::string& section, const std::string& name)
{
    std::string regStr = registry.Get(section, name);
    return (regStr.size() > 0);
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

bool RegistryGetString(const std::string& section, const std::string& name, std::string *value)
{
    std::string regStr = registry.Get(section, name);
    if (regStr.size() == 0) {
        ERR_POST(Warning << "Can't get string from registry: " << section << ", " << name);
        return false;
    }
    *value = regStr;
    return true;
}

bool RegistrySetInteger(const std::string& section, const std::string& name, int value)
{
    wxString regStr;
    regStr.Printf("%i", value);
    bool okay = registry.Set(section, name, regStr.c_str(), CNcbiRegistry::ePersistent);
    if (!okay)
        ERR_POST(Error << "registry Set(" << section << ", " << name << ") failed");
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
        ERR_POST(Error << "registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetString(const std::string& section, const std::string& name, const std::string& value)
{
    bool okay = registry.Set(section, name, value, CNcbiRegistry::ePersistent);
    if (!okay)
        ERR_POST(Error << "registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

END_SCOPE(Cn3D)

