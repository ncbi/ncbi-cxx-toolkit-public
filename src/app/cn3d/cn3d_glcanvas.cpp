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
*      OpenGL canvas object
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <memory>

#include <wx/fontutil.h>
#include <wx/image.h>

#include "cn3d_glcanvas.hpp"
#include "opengl_renderer.hpp"
#include "cn3d_tools.hpp"
#include "structure_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

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
    string nativeFont;
    if (!RegistryGetString(REG_OPENGL_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO, &nativeFont))
    {
        ERRORMSG("Cn3DGLCanvas::SetGLFontFromRegistry() - error getting font info from registry");
        return;
    }

    // create new font - assignment uses object reference to copy
    wxNativeFontInfo fontInfo;
    if (!fontInfo.FromString(nativeFont.c_str())) {
        ERRORMSG("Cn3DGLCanvas::SetGLFontFromRegistry() - can't set wxNativeFontInfo fron native font string");
        return;
    }
#if wxCHECK_VERSION(2,3,4)
    if (fontScale != 1.0 && fontScale > 0.0)
        fontInfo.SetPointSize(fontScale * fontInfo.GetPointSize());
#endif
    auto_ptr<wxFont> newFont(wxFont::New(fontInfo));
    if (!newFont.get() || !newFont->Ok()) {
        ERRORMSG("Cn3DGLCanvas::SetGLFontFromRegistry() - can't get wxFont from wxNativeFontInfo");
        return;
    }
    font = *newFont;    // copy font

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

bool Cn3DGLCanvas::MeasureText(const string& text, int *width, int *height, int *centerX, int *centerY)
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
    wxImage image(memoryBitmap.ConvertToImage());
//    wxInitAllImageHandlers();
//    image.SaveFile("text.png", wxBITMAP_TYPE_PNG); // for testing

    // now find extent of actual (red) text pixels; wx coords put (0,0) at upper left
    int x, y, top = image.GetHeight(), left = image.GetWidth(), bottom = -1, right = -1;
    for (x=0; x<image.GetWidth(); ++x) {
        for (y=0; y<image.GetHeight(); ++y) {
            if (image.GetRed(x, y) >= 128) { // character pixel here
                if (y < top) top = y;
                if (x < left) left = x;
                if (y > bottom) bottom = y;
                if (x > right) right = x;
            }
        }
    }
    if (bottom < 0 || right < 0) {
        WARNINGMSG("Cn3DGLCanvas::MeasureText() - no character pixels found!");
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
#if !defined(__WXGTK__)
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

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/03/15 18:23:01  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.3  2004/02/19 17:04:50  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.2  2003/03/14 19:22:59  thiessen
* add CommandProcessor to handle file-message commands; fixes for GCC 2.9
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/
