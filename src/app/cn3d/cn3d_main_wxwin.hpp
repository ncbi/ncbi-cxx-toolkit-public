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

#include <wx/wx.h>
#if !wxUSE_GLCANVAS
#error Please set wxUSE_GLCANVAS to 1 in setup.h.
#endif
#include <wx/glcanvas.h>

#include "cn3d/opengl_renderer.hpp"


BEGIN_SCOPE(Cn3D)

class Cn3DMainFrame;
class SequenceViewer;
class Messenger;

// Define a new application type
class Cn3DApp: public wxApp
{
public:
    bool OnInit(void);
    int OnExit(void);

    // used for processing display updates when system is idle
    void OnIdle(wxIdleEvent& event);

    // for now, there is only one structure window
    Cn3DMainFrame *structureWindow;

    // for now, there is only one sequence viewer
    SequenceViewer *sequenceViewer;

    DECLARE_EVENT_TABLE()
};

class Cn3DGLCanvas;

class Cn3DMainFrame: public wxFrame
{
    friend class Cn3DApp;

public:
    Cn3DMainFrame(
        wxFrame *parent, const wxString& title,
        const wxPoint& pos, const wxSize& size, long style = wxDEFAULT_FRAME_STYLE);
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
            MID_SECSTRUC,
            MID_ALIGN,
                // conservation sub-menu
                MID_CONS,
                    MID_IDENT,
                    MID_VARIETY,
                    MID_WGHT_VAR,
                    MID_FIT,
            MID_WIREFRAME,
        // Quality menu
            MID_QLOW,
            MID_QMED,
            MID_QHIGH
    };

    void OnExit(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    void OnOpen(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);

    void OnAlignStructures(wxCommandEvent& event);

    void OnAdjustView(wxCommandEvent& event);
    void OnShowHide(wxCommandEvent& event);
    void OnSetStyle(wxCommandEvent& event);
    void OnSetQuality(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()

private:
    wxString workingDir, programDir, dataDir;
};

class Cn3DGLCanvas: public wxGLCanvas
{
public:
    Cn3DGLCanvas(
        wxWindow *parent, const wxWindowID id = -1, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0, const wxString& name = "Cn3DGLCanvas",
        int *gl_attrib = NULL);
    ~Cn3DGLCanvas(void);

    // public data
    StructureSet *structureSet;
    OpenGLRenderer renderer;
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
