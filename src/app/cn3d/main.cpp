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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <serial/serial.hpp>            
#include <serial/objistrasnb.hpp>       
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>

// For now, this module will contain a simple wxWindows + wxGLCanvas interface
#include "cn3d/main.hpp"

USING_NCBI_SCOPE;
using namespace objects;
using namespace Cn3D;

static wxFrame *logFrame = NULL;
static wxTextCtrl *logText = NULL;

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    std::string errMsg;
    diagMsg.Write(errMsg);

    if (diagMsg.m_Severity >= eDiag_Error) {
        wxMessageDialog *dlg =
			new wxMessageDialog(NULL, errMsg.c_str(), "Severe Error!",
				wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg->ShowModal();
		delete dlg;
    } else {
        if (!logFrame) {
            logFrame = new wxFrame(NULL, -1, "Cn3D++ Message Log", 
                wxPoint(50, 50), wxSize(400, 200),
                wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCAPTION | wxTHICK_FRAME);
            logFrame->SetSizeHints(150, 100);
            logText = new wxTextCtrl(logFrame, -1, "", 
                wxPoint(0,0), wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        }
        logText->AppendText(errMsg.c_str());
        logFrame->Show(TRUE);
    }
}

// `Main program' equivalent, creating windows and returning main app frame
IMPLEMENT_APP(Cn3DApp)

bool Cn3DApp::OnInit(void)
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info); // report all messages

    // Create the main frame window
    frame = new Cn3DMainFrame(NULL, "Cn3D++", wxPoint(50, 50), wxSize(400, 400),
        wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION);

    // get file name from command line for now
    if (argc > 2)
        ERR_POST(Fatal << "\nUsage: cn3d [filename]");
    else if (argc == 2) {
        frame->LoadFile(argv[1]);
    }

    return TRUE;
}

BEGIN_EVENT_TABLE(Cn3DMainFrame, wxFrame)
    EVT_MENU(MID_OPEN, Cn3DMainFrame::OnOpen)
    EVT_MENU(MID_EXIT, Cn3DMainFrame::OnExit)
END_EVENT_TABLE()

// frame constructor
Cn3DMainFrame::Cn3DMainFrame(wxFrame *frame, const wxString& title, const wxPoint& pos,
    const wxSize& size, long style) :
    wxFrame(frame, -1, title, pos, size, style | wxTHICK_FRAME), structureSet(NULL)
{
    SetSizeHints(150, 150);

    // Make a menubar
    menuBar = new wxMenuBar;
    menu1 = new wxMenu;
    menu1->Append(MID_OPEN, "&Open");
    menu1->AppendSeparator();
    menu1->Append(MID_EXIT, "E&xit");
    menuBar->Append(menu1, "&File");
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
    glCanvas = 
        new Cn3DGLCanvas(this, -1, wxPoint(0, 0), wxSize(400, 400), 0, "Cn3DGLCanvas", gl_attrib);
    glCanvas->SetCurrent();

    // Show the frame
    Show(TRUE);

    // Centre frame on the screen.
    Centre(wxBOTH);
}

void Cn3DMainFrame::OnExit(wxCommandEvent& event)
{
	Destroy();
}

Cn3DMainFrame::~Cn3DMainFrame(void)
{
    if (structureSet) delete structureSet;
    delete glCanvas;
    if (logFrame) {
        delete logText;
        delete logFrame;
        logText = NULL;
        logFrame = NULL;
    }
}

void Cn3DMainFrame::LoadFile(const char *filename)
{
    // clear old data
    if (structureSet) {
        delete structureSet;
        structureSet = NULL;
    }

    // initialize the binary input stream 
    auto_ptr<CNcbiIstream> inStream;
    inStream.reset(new CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary));
    if ( !*inStream )
        ERR_POST(Fatal << "Cannot open input file '" << filename << "'");

    // Associate ASN.1 binary serialization methods with the input 
    auto_ptr<CObjectIStream> inObject;
    inObject.reset(new CObjectIStreamAsnBinary(*inStream));

    // Read the CNcbi_mime_asn1 data 
    CNcbi_mime_asn1 mime;
    *inObject >> mime;

    // Create StructureSet from mime data
    structureSet = new StructureSet(mime);
}

void Cn3DMainFrame::OnOpen(wxCommandEvent& event)
{
    const wxString& filestr = wxFileSelector("Choose a binary ASN1 file to open");
    if (filestr.IsEmpty()) return;
    LoadFile(filestr.c_str());
}


/*
 * Cn3DGLCanvas implementation
 */

BEGIN_EVENT_TABLE(Cn3DGLCanvas, wxGLCanvas)
    EVT_SIZE(Cn3DGLCanvas::OnSize)
    EVT_PAINT(Cn3DGLCanvas::OnPaint)
    //EVT_CHAR(Cn3DGLCanvas::OnChar)
    EVT_MOUSE_EVENTS(Cn3DGLCanvas::OnMouseEvent)
    EVT_ERASE_BACKGROUND(Cn3DGLCanvas::OnEraseBackground)
END_EVENT_TABLE()

Cn3DGLCanvas::Cn3DGLCanvas(wxWindow *parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name, int *gl_attrib) :
    wxGLCanvas(parent, id, pos, size, style, name, gl_attrib)
{
    parent->Show(TRUE);
    SetCurrent();
}

Cn3DGLCanvas::~Cn3DGLCanvas(void)
{
}

void Cn3DGLCanvas::OnPaint(wxPaintEvent& event)
{
    // This is a dummy, to avoid an endless succession of paint messages.
    // OnPaint handlers must always create a wxPaintDC.
    wxPaintDC dc(this);

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

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
    glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
}

void Cn3DGLCanvas::OnMouseEvent(wxMouseEvent& event)
{
    static int dragging = 0;
    static float last_x, last_y;

    if (event.LeftIsDown()) {
        if (!dragging) {
            dragging = 1;
        } else {
            yrot += (event.GetX() - last_x)*1.0;
            xrot += (event.GetY() - last_y)*1.0;
            Refresh(FALSE);
        }
        last_x = event.GetX();
        last_y = event.GetY();
    } else
        dragging = 0;
}

void Cn3DGLCanvas::OnEraseBackground(wxEraseEvent& event)
{
    // Do nothing, to avoid flashing.
}
