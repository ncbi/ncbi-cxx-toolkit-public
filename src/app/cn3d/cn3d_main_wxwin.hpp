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
* Revision 1.56  2002/01/11 15:48:55  thiessen
* update for Mac CW7
*
* Revision 1.55  2002/01/03 16:18:40  thiessen
* add distance selection
*
* Revision 1.54  2001/12/21 13:52:20  thiessen
* add spin animation
*
* Revision 1.53  2001/12/14 14:52:45  thiessen
* add apple event handler for open document
*
* Revision 1.52  2001/10/24 22:02:02  thiessen
* fix wxGTK concurrent rendering problem
*
* Revision 1.51  2001/10/23 20:10:23  thiessen
* fix scaling of fonts in high-res PNG output
*
* Revision 1.50  2001/10/23 13:53:37  thiessen
* add PNG export
*
* Revision 1.49  2001/10/17 17:46:27  thiessen
* save camera setup and rotation center in files
*
* Revision 1.48  2001/10/16 21:48:28  thiessen
* restructure MultiTextDialog; allow virtual bonds for alpha-only PDB's
*
* Revision 1.47  2001/10/11 14:18:20  thiessen
* make MultiTextDialog non-modal
*
* Revision 1.46  2001/10/09 18:57:27  thiessen
* add CDD references editing dialog
*
* Revision 1.45  2001/10/08 00:00:02  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.44  2001/10/01 16:03:58  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.43  2001/09/27 15:36:48  thiessen
* decouple sequence import and BLAST
*
* Revision 1.42  2001/09/26 15:27:59  thiessen
* tweak sequence viewer widget for wx2.3.2, tweak cdd annotation
*
* Revision 1.41  2001/09/24 16:43:10  thiessen
* add wxGLApp test code
*
* Revision 1.40  2001/09/06 21:38:33  thiessen
* tweak message log / diagnostic system
*
* Revision 1.39  2001/09/04 14:40:26  thiessen
* add rainbow and charge coloring
*
* Revision 1.38  2001/08/31 22:24:14  thiessen
* add timer for animation
*
* Revision 1.37  2001/08/24 00:40:57  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.36  2001/08/14 17:17:48  thiessen
* add user font selection, store in registry
*
* Revision 1.35  2001/08/10 15:02:03  thiessen
* fill out shortcuts; add update show/hide menu
*
* Revision 1.34  2001/08/09 19:07:19  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.33  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.32  2001/08/03 13:41:24  thiessen
* add registry and style favorites
*
* Revision 1.31  2001/07/12 17:34:22  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.30  2001/07/10 16:39:33  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.29  2001/06/29 18:12:53  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.28  2001/06/14 17:44:46  thiessen
* progress in styles<->asn ; add structure limits
*
* Revision 1.27  2001/05/31 18:46:25  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.26  2001/05/25 15:17:49  thiessen
* fixes for visual id settings in wxGTK
*
* Revision 1.25  2001/05/21 22:06:56  thiessen
* fix initial glcanvas size bug
*
* Revision 1.24  2001/05/17 18:34:00  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.23  2001/05/15 17:33:58  thiessen
* log window stays hidden when closed
*
* Revision 1.22  2001/05/11 02:10:04  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.21  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.20  2001/03/17 14:06:52  thiessen
* more workarounds for namespace/#define conflicts
*
* Revision 1.19  2001/02/22 00:29:48  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.18  2001/02/13 20:31:45  thiessen
* add information content coloring
*
* Revision 1.17  2001/02/08 23:01:13  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.16  2001/01/18 19:37:00  thiessen
* save structure (re)alignments to asn output
*
* Revision 1.15  2000/12/29 19:23:49  thiessen
* save row order
*
* Revision 1.14  2000/12/22 19:25:46  thiessen
* write cdd output files
*
* Revision 1.13  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.12  2000/11/30 15:49:08  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.11  2000/11/13 18:05:58  thiessen
* working structure re-superpositioning
*
* Revision 1.10  2000/11/12 04:02:22  thiessen
* working file save including alignment edits
*
* Revision 1.9  2000/10/16 14:25:19  thiessen
* working alignment fit coloring
*
* Revision 1.8  2000/09/20 22:22:02  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.7  2000/09/12 01:46:07  thiessen
* fix minor but obscure bug
*
* Revision 1.6  2000/09/11 14:06:02  thiessen
* working alignment coloring
*
* Revision 1.5  2000/09/11 01:45:53  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.4  2000/09/03 18:45:56  thiessen
* working generalized sequence viewer
*
* Revision 1.3  2000/08/30 19:49:03  thiessen
* working sequence window
*
* Revision 1.2  2000/08/27 18:50:55  thiessen
* extract sequence information
*
* Revision 1.1  2000/08/25 18:41:54  thiessen
* rename main object
*
* Revision 1.10  2000/08/25 14:21:32  thiessen
* minor tweaks
*
* Revision 1.9  2000/08/24 23:39:54  thiessen
* add 'atomic ion' labels
*
* Revision 1.8  2000/08/17 18:32:37  thiessen
* minor fixes to StyleManager
*
* Revision 1.7  2000/08/07 14:12:47  thiessen
* added animation frames
*
* Revision 1.6  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.5  2000/07/18 16:49:43  thiessen
* more friendly rotation center setting
*
* Revision 1.4  2000/07/12 23:28:27  thiessen
* now draws basic CPK model
*
* Revision 1.3  2000/07/12 14:10:44  thiessen
* added initial OpenGL rendering engine
*
* Revision 1.2  2000/07/12 02:00:39  thiessen
* add basic wxWindows GUI
*
* Revision 1.1  2000/06/27 20:08:12  thiessen
* initial checkin
*
* ===========================================================================
*/

#ifndef CN3D_MAIN__HPP
#define CN3D_MAIN__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>
#if !wxUSE_GLCANVAS
#error Please set wxUSE_GLCANVAS to 1 in setup.h.
#endif
#include <wx/glcanvas.h>

// for testing my new wxGLApp class
#define TEST_WXGLAPP 1
#if defined(TEST_WXGLAPP) && wxVERSION_NUMBER < 2302
#undef TEST_WXGLAPP
#endif

#include "cn3d/multitext_dialog.hpp"


BEGIN_SCOPE(Cn3D)

class Cn3DMainFrame;
class SequenceViewer;
class OpenGLRenderer;
class StructureSet;

// Define a new application type
#ifdef TEST_WXGLAPP
class Cn3DApp: public wxGLApp
#else
class Cn3DApp: public wxApp
#endif
{
public:
    Cn3DApp();

    bool OnInit(void);
    int OnExit(void);

    // for now, there is only one structure window
    Cn3DMainFrame *structureWindow;

    // for now, there is only one sequence viewer
    SequenceViewer *sequenceViewer;

private:
    void InitRegistry(void);
#ifdef __WXMAC__
    short MacHandleAEODoc(const WXAPPLEEVENTREF event, WXAPPLEEVENTREF reply);
#endif

    // used for processing display updates when system is idle
    void OnIdle(wxIdleEvent& event);

    DECLARE_EVENT_TABLE()
};


class Cn3DGLCanvas;
class CDDAnnotateDialog;
class MultiTextDialog;

class Cn3DMainFrame: public wxFrame, public MultiTextDialogOwner
{
public:
    Cn3DMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~Cn3DMainFrame();

    // public data
    Cn3DGLCanvas *glCanvas;

    // public methods
    void LoadFile(const char *filename);
    bool SaveDialog(bool canCancel);
    void DialogTextChanged(const MultiTextDialog *changed);
    void DialogDestroyed(const MultiTextDialog *destroyed);

    enum {
        // File menu
            MID_OPEN,
            MID_SAVE,
            MID_PNG,
            MID_REFIT_ALL,
            MID_LIMIT_STRUCT,
            MID_PREFERENCES,
            MID_FONTS,
                MID_OPENGL_FONT,
                MID_SEQUENCE_FONT,
            MID_EXIT,
        // View menu
            MID_ZOOM_IN,
            MID_ZOOM_OUT,
            MID_RESET,
            MID_RESTORE,
            MID_NEXT_FRAME,
            MID_PREV_FRAME,
            MID_FIRST_FRAME,
            MID_LAST_FRAME,
            MID_ALL_FRAMES,
            MID_ANIMATE,
                MID_PLAY,
                MID_SPIN,
                MID_STOP,
                MID_SET_DELAY,
        // Show/Hide menu
            MID_SHOW_HIDE,
            MID_SHOW_ALL,
            MID_SHOW_DOMAINS,
            MID_SHOW_ALIGNED,
            MID_SHOW_UNALIGNED,
                MID_SHOW_UNALIGNED_ALL,
                MID_SHOW_UNALIGNED_ALN_DOMAIN,
            MID_SHOW_SELECTED,
            MID_DIST_SELECT,
                MID_DIST_SELECT_RESIDUES,
                MID_DIST_SELECT_ALL,
        // Structure Alignments menu
        // Style menu
            MID_EDIT_STYLE,
            MID_RENDER, // rendering shortcuts
                MID_WORM,
                MID_TUBE,
                MID_WIRE,
                MID_BNS,
                MID_SPACE,
                MID_SC_TOGGLE,
            MID_COLORS, // color shortcuts
                MID_SECSTRUC,
                MID_ALIGNED,
                MID_CONS,
                    MID_IDENTITY,
                    MID_VARIETY,
                    MID_WGHT_VAR,
                    MID_INFO,
                    MID_FIT,
                MID_OBJECT,
                MID_DOMAIN,
                MID_MOLECULE,
                MID_RAINBOW,
                MID_HYDROPHOB,
                MID_CHARGE,
                MID_TEMP,
                MID_ELEMENT,
            MID_ANNOTATE,
            MID_FAVORITES, // favorites submenu
                MID_ADD_FAVORITE,
                MID_REMOVE_FAVORITE,
                MID_FAVORITES_FILE,
        // Window menu
            MID_SHOW_LOG,
            MID_SHOW_LOG_START,
            MID_SHOW_SEQ_V,
        // CDD menu
            MID_EDIT_CDD_NAME,
            MID_EDIT_CDD_DESCR,
            MID_EDIT_CDD_NOTES,
            MID_EDIT_CDD_REFERENCES,
            MID_ANNOT_CDD,

        // not actually a menu item, but used to enumerate style favorites list
            MID_FAVORITES_BEGIN = 100,
            MID_FAVORITES_END   = 149
    };

private:

    // non-modal dialogs owned by this object
    CDDAnnotateDialog *cddAnnotateDialog;
    MultiTextDialog *cddDescriptionDialog, *cddNotesDialog;
    void DestroyNonModalDialogs(void);

    void OnExit(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
    void OnShowWindow(wxCommandEvent& event);

    // menu-associated methods
    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnPNG(wxCommandEvent& event);
    void OnLimit(wxCommandEvent& event);
    void OnAlignStructures(wxCommandEvent& event);
    void OnAdjustView(wxCommandEvent& event);
    void OnShowHide(wxCommandEvent& event);
    void OnDistanceSelect(wxCommandEvent& event);
    void OnSetStyle(wxCommandEvent& event);
    void OnEditFavorite(wxCommandEvent& event);
    void OnSelectFavorite(wxCommandEvent& event);
    void OnCDD(wxCommandEvent& event);
    void OnPreferences(wxCommandEvent& event);
    void OnSetFont(wxCommandEvent& event);
    void OnAnimate(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);

    static const int UNLIMITED_STRUCTURES;
    int structureLimit;

    wxMenuBar *menuBar;
    wxMenu *favoritesMenu;

    wxTimer timer;
    enum { ANIM_FRAMES, ANIM_SPIN };    // animation modes
    int animationMode;

    DECLARE_EVENT_TABLE()
};


class Cn3DGLCanvas: public wxGLCanvas
{
public:
    Cn3DGLCanvas(wxWindow *parent, int *attribList);
    ~Cn3DGLCanvas(void);

    StructureSet *structureSet;
    OpenGLRenderer *renderer;

    // font stuff - setup from registry, and measure using currently selected font
    void SetGLFontFromRegistry(double fontScale = 1.0);
    bool MeasureText(const std::string& text, int *width, int *height, int *centerX, int *centerY);
    const wxFont& GetGLFont(void) const { return font; }

    void SuspendRendering(bool suspend);

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnMouseEvent(wxMouseEvent& event);

    // memoryDC is used for measuring text
    wxMemoryDC memoryDC;
    wxBitmap memoryBitmap;
    wxFont font;
    bool suspended;

    DECLARE_EVENT_TABLE()
};


END_SCOPE(Cn3D)

#endif // CN3D_MAIN__HPP
