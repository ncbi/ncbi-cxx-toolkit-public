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


BEGIN_SCOPE(Cn3D)

class Cn3DMainFrame;
class SequenceViewer;
class OpenGLRenderer;
class StructureSet;

// Define a new application type
class Cn3DApp: public wxApp
{
public:
    Cn3DApp();

    bool OnInit(void);
    int OnExit(void);

    // used for processing display updates when system is idle
    void OnIdle(wxIdleEvent& event);

    // for now, there is only one structure window
    Cn3DMainFrame *structureWindow;

    // for now, there is only one sequence viewer
    SequenceViewer *sequenceViewer;

private:
    DECLARE_EVENT_TABLE()
};

class Cn3DGLCanvas;

class Cn3DMainFrame: public wxFrame
{
public:
    Cn3DMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~Cn3DMainFrame();

    // public data
    Cn3DGLCanvas *glCanvas;

    // public methods
    void LoadFile(const char *filename);
    bool SaveDialog(bool canCancel);

    // menu-associated methods
    enum {
        // File menu
            MID_OPEN,
            MID_SAVE,
            MID_LIMIT_STRUCT,
            MID_EXIT,
        // View menu
            MID_TRANSLATE,
            MID_ZOOM_IN,
            MID_ZOOM_OUT,
            MID_RESET,
        // Show/Hide menu
            MID_SHOW_HIDE,
            MID_SHOW_ALL,
            MID_SHOW_DOMAINS,
            MID_SHOW_ALIGNED,
            MID_SHOW_UNALIGNED,
            MID_SHOW_SELECTED,
        // Structure Alignments menu
            MID_REFIT_ALL,
        // Style menu
            MID_EDIT_STYLE,
            MID_RENDER, // rendering shortcuts
                MID_WORM,
                MID_TUBE,
                MID_WIRE,
            MID_COLORS, // color shortcuts
                MID_SECSTRUC,
                MID_ALIGNED,
                MID_INFO,
            MID_ANNOTATE,
        // Quality menu
            MID_QLOW,
            MID_QMED,
            MID_QHIGH,
        // Window menu
            MID_SHOW_LOG,
            MID_SHOW_SEQ_V,
        // CDD menu
           MID_EDIT_CDD_DESCR,
           MID_EDIT_CDD_NOTES,
           MID_ANNOT_CDD
    };

    void OnExit(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    void OnShowWindow(wxCommandEvent& event);

    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnLimit(wxCommandEvent& event);

    void OnAlignStructures(wxCommandEvent& event);

    void OnAdjustView(wxCommandEvent& event);
    void OnShowHide(wxCommandEvent& event);
    void OnSetStyle(wxCommandEvent& event);
    void OnSetQuality(wxCommandEvent& event);

    void OnCDD(wxCommandEvent& event);

private:

    static const int UNLIMITED_STRUCTURES;
    int structureLimit;

    wxMenuBar *menuBar;

    DECLARE_EVENT_TABLE()
};

class Cn3DGLCanvas: public wxGLCanvas
{
public:
    Cn3DGLCanvas(wxWindow *parent, int *attribList);
    ~Cn3DGLCanvas(void);

    // public data
    StructureSet *structureSet;
    OpenGLRenderer *renderer;
    wxFont *font;

    // public methods
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnMouseEvent(wxMouseEvent& event);

private:
    DECLARE_EVENT_TABLE()
};


END_SCOPE(Cn3D)

#endif // CN3D_MAIN__HPP
