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
*      Classes to hold the OpenGL rendering engine
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/07/12 14:10:45  thiessen
* added initial OpenGL rendering engine
*
* ===========================================================================
*/

#ifndef CN3D_OPENGL_RENDERER__HPP
#define CN3D_OPENGL_RENDERER__HPP

#if defined(WIN32)
#include <windows.h>

#elif defined(macintosh)
#include <agl.h>

#elif defined(WIN_MOTIF)
#include <GL/glx.h>

#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include <corelib/ncbistl.hpp>

BEGIN_SCOPE(Cn3D)

class StructureSet;

class OpenGLRenderer
{
public:
    OpenGLRenderer(void);

    // public data

    // public methods

    // calls once-only OpenGL initialization stuff (should be called after
    // the rendering context is established and the renderer made current)
    void Init(void) const;

    // tells the renderer that new camera settings need to be applied
    void NewView(void) const;

    // tells the renderer what structure(s) it's to draw
    void AttachStructureSet(StructureSet *targetStructureSet);

    // constructs the display lists (but doesn't draw them)
    void Construct(void);

    // draws the display lists
    void Display(void) const;

    // sets the size of the viewport
    void SetSize(GLint width, GLint height) const;

    // called when mouse is click-dragged in window
    void MouseDragged(int dY, int dX);

private:
    StructureSet *structureSet;

    // camera data
    GLdouble cameraDistance, cameraAngleRad, 
        cameraLookAtX, cameraLookAtY,
        cameraClipNear, cameraClipFar;

    // background color
    GLclampf background[3];
};

END_SCOPE(Cn3D)

#endif // CN3D_OPENGL_RENDERER__HPP
