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

// For now, this module will contain a simple wxWindows + wxGLCanvas interface

#include <wx/wx.h>

#if !wxUSE_GLCANVAS
#error Please set wxUSE_GLCANVAS to 1 in setup.h.
#endif
#include <wx/glcanvas.h>

#include "cn3d/structure_set.hpp"
#include "cn3d/opengl_renderer.hpp"

using namespace Cn3D;

class Cn3DMainFrame;

// Define a new application type
class Cn3DApp: public wxApp
{
public:
    bool OnInit(void);
    Cn3DMainFrame *frame;
};

class Cn3DGLCanvas: public wxGLCanvas
{
public:
    Cn3DGLCanvas(wxWindow *parent, const wxWindowID id = -1, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize, long style = 0, const wxString& name = "Cn3DGLCanvas",
        int *gl_attrib = NULL);
    ~Cn3DGLCanvas(void);

    // public data
    StructureSet *structureSet;
    OpenGLRenderer renderer;

    // public methods
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);
    //void OnChar(wxKeyEvent& event);
    void OnMouseEvent(wxMouseEvent& event);

DECLARE_EVENT_TABLE()
};

class Cn3DMainFrame: public wxFrame
{
public:
    Cn3DMainFrame(wxFrame *frame, const wxString& title, const wxPoint& pos, const wxSize& size,
        long style = wxDEFAULT_FRAME_STYLE);
    ~Cn3DMainFrame();

    // public data
    Cn3DGLCanvas *glCanvas;
    wxMenuBar *menuBar;
    wxMenu *menu1;

    // public methods
    void LoadFile(const char *filename);

    void OnOpen(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);

DECLARE_EVENT_TABLE()
};

/* 
 * Menu identifiers
 */
enum {
    MENU1_FILE,         // File
        MID_OPEN,                   // Open
        MID_EXIT                    // Exit
};

#endif // CN3D_MAIN__HPP
