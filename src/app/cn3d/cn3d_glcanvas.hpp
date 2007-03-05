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

#ifndef CN3D_GLCANVAS__HPP
#define CN3D_GLCANVAS__HPP

#include <corelib/ncbistd.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/glcanvas.h>


BEGIN_SCOPE(Cn3D)

class StructureSet;
class OpenGLRenderer;

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
    void FakeOnSize(void);

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

#endif // CN3D_GLCANVAS__HPP
