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
* Revision 1.1  2000/07/12 14:11:30  thiessen
* added initial OpenGL rendering engine
*
* ===========================================================================
*/

#include <math.h>

#include "cn3d/opengl_renderer.hpp"
#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

// local functions
static void ConstructLogo(void);
static void SetColor(GLenum type, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha = 1.0);

static const double PI = acos(-1);
inline double DegreesToRad(double deg) { return deg*PI/180.0; }
inline double RadToDegrees(double rad) { return rad*180.0/PI; }

/* these are used for both matrial colors and light colors */
static const GLfloat Color_Off[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static const GLfloat Color_MostlyOff[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
static const GLfloat Color_MostlyOn[4] = { 0.95f, 0.95f, 0.95f, 1.0f };
static const GLfloat Color_On[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

OpenGLRenderer::OpenGLRenderer(void)
{
    AttachStructureSet(NULL);
}

void OpenGLRenderer::Init(void) const
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // set up the lighting
    // directional light (faster) when LightPosition[4] == 0.0
    GLfloat LightPosition[4] = { 0.0, 0.0, 1.0, 0.0 };
    glLightfv(GL_LIGHT0, GL_POSITION, LightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, Color_Off);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, Color_MostlyOn);
    glLightfv(GL_LIGHT0, GL_SPECULAR, Color_Off);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, Color_On); // global ambience
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    /* clear these material colors */
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, Color_Off);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, Color_Off);

    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);

    /* turn on culling to speed rendering */
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void OpenGLRenderer::NewView(void) const
{
    GLint Viewport[4];
    glGetIntegerv(GL_VIEWPORT, Viewport);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    GLdouble aspect = ((GLfloat)(Viewport[2])) / Viewport[3];
    gluPerspective(RadToDegrees(cameraAngleRad),    //viewing angle (degrees)
                   aspect,                          // w/h aspect
                   cameraClipNear,                  // near clipping plane
                   cameraClipFar);                  //far clipping plane 
    gluLookAt(0.0,0.0,cameraDistance,               // the camera position
                cameraLookAtX,                      // the "look-at" point
                cameraLookAtY,
                0.0,
                0.0,1.0,0.0);                      // the up direction

    glMatrixMode(GL_MODELVIEW);
}

void OpenGLRenderer::AttachStructureSet(StructureSet *targetStructureSet)
{
    structureSet = targetStructureSet;

    // make sure general GL stuff is set up
    background[0] = background[1] = background[2] = 0.0;
    Init();
    
    // set up initial camera
    cameraLookAtX = cameraLookAtY = 0.0;
    if (structureSet) { // for structure
        cameraAngleRad = DegreesToRad(35.0);
    } else { // for logo
        cameraAngleRad = PI / 14;
        cameraDistance = 200;
        ConstructLogo();
    }
    cameraClipNear = 0.5*cameraDistance;
    cameraClipFar = 1.5*cameraDistance;
    NewView();
}

void OpenGLRenderer::SetSize(GLint width, GLint height) const
{
    glViewport(0, 0, width, height);
    NewView();
}

void OpenGLRenderer::MouseDragged(int dY, int dX)
{
}

void OpenGLRenderer::Display(void) const
{
    glClearColor(background[0], background[1], background[2], 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (structureSet) {
    } else {
        glCallList(1); // draw logo
    }
    glFlush();
}

static void SetColor(GLenum type, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    static GLfloat pr, pg, pb, pa;
    static GLenum pt = GL_NONE;
    static GLfloat rgb[4] = { red, green, blue, alpha};

//#ifdef _DEBUG
    if (red == 0.0 && green == 0.0 && blue == 0.0)
        ERR_POST(Warning << "SetColor request color (0,0,0)");
    if (alpha == 0.0)
        ERR_POST(Warning << "SetColor request alpha 0.0");
//#endif

    if (rgb[0] != pr || rgb[1] != pg || rgb[2] != pb || rgb[3] != pa || type != pt) {
        if (type != pt) {
            if (type == GL_DIFFUSE) {
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, Color_MostlyOff);
            } else if (type == GL_AMBIENT) {
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, Color_Off);
            } else {
                ERR_POST(Error << "don't know how to handle material type " << type);
            }
            pt = type;
        }
        glMaterialfv(GL_FRONT_AND_BACK, type, rgb);
        if (type == GL_AMBIENT) {
            /* this is necessary so that fonts are rendered in correct
                color in SGI's OpenGL implementation, and maybe others */
            glColor4f(rgb[0], rgb[1], rgb[2], rgb[3]);
        }
        pr = rgb[0];
        pg = rgb[1];
        pb = rgb[2];
        pa = rgb[3];
    }
}

/* create display list with logo */
static void ConstructLogo(void)
{
    GLfloat logoColor[3] = { 100.0f/255, 240.0f/255, 150.0f/255 };
    int i, n, s, g;

    static const int LOGO_SIDES = 36;
    int segments = 180;
    GLdouble bigRad = 12.0, height = 24.0,
        minRad = 0.1, maxRad = 2.0,
        ringPts[LOGO_SIDES * 3], *pRingPts = ringPts,
        prevRing[LOGO_SIDES * 3], *pPrevRing = prevRing, *tmp,
        ringNorm[LOGO_SIDES * 3], *pRingNorm = ringNorm,
        prevNorm[LOGO_SIDES * 3], *pPrevNorm = prevNorm,
        PI = acos(-1), length,
        startRad, midRad, phase, currentRad, CR[3], H[3], V[3];


    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glNewList(1, GL_COMPILE);

    /* create logo */
    SetColor(GL_DIFFUSE, logoColor[0], logoColor[1], logoColor[2]);

    for (n = 0; n < 2; n++) { /* helix strand */
        if (n == 0) {
            startRad = maxRad;
            midRad = minRad;
            phase = 0;
        } else {
            startRad = minRad;
            midRad = maxRad;
            phase = PI;
        }
        for (g = 0; g <= segments; g++) { /* segment (bottom to top) */

            if (g < segments/2)
                currentRad = startRad + (midRad - startRad) *
                    (0.5 - 0.5 * cos(PI * g / (segments/2)));
            else
                currentRad = midRad + (startRad - midRad) *
                    (0.5 - 0.5 * cos(PI * (g - segments/2) / (segments/2)));

            CR[1] = height * g / segments - height/2;
            if (g > 0) phase += PI * 2 / segments;
            CR[2] = bigRad * cos(phase);
            CR[0] = bigRad * sin(phase);

            /* make a strip around the strand circumference */
            for (s = 0; s < LOGO_SIDES; s++) {
                V[0] = CR[0];
                V[2] = CR[2];
                V[1] = 0;
                length = sqrt(V[0]*V[0] + V[1]*V[1] + V[2]*V[2]);
                for (i = 0; i < 3; i++) V[i] /= length;
                H[0] = H[2] = 0;
                H[1] = 1;
                for (i = 0; i < 3; i++) {
                    pRingNorm[3*s + i] = V[i] * cos(PI * 2 * s / LOGO_SIDES) +
                                         H[i] * sin(PI * 2 * s / LOGO_SIDES);
                    pRingPts[3*s + i] = CR[i] + pRingNorm[3*s + i] * currentRad;
                }
            }
            if (g > 0) {
                glBegin(GL_TRIANGLE_STRIP);
                for (s = 0; s < LOGO_SIDES; s++) {
                    glNormal3d(pPrevNorm[3*s], pPrevNorm[3*s + 1], pPrevNorm[3*s + 2]);
                    glVertex3d(pPrevRing[3*s], pPrevRing[3*s + 1], pPrevRing[3*s + 2]);
                    glNormal3d(pRingNorm[3*s], pRingNorm[3*s + 1], pRingNorm[3*s + 2]);
                    glVertex3d(pRingPts[3*s], pRingPts[3*s + 1], pRingPts[3*s + 2]);
                }
                glNormal3d(pPrevNorm[0], pPrevNorm[1], pPrevNorm[2]);
                glVertex3d(pPrevRing[0], pPrevRing[1], pPrevRing[2]);
                glNormal3d(pRingNorm[0], pRingNorm[1], pRingNorm[2]);
                glVertex3d(pRingPts[0], pRingPts[1], pRingPts[2]);
                glEnd();
            }
            
            /* cap the ends */
            glBegin(GL_POLYGON);
            if ((g == 0 && n == 0) || (g == segments && n == 1))
                glNormal3d(-1, 0, 0);
            else
                glNormal3d(1, 0, 0);
            if (g == 0) {
                for (s = 0; s < LOGO_SIDES; s++)
                    glVertex3d(pRingPts[3*s], pRingPts[3*s + 1], pRingPts[3*s + 2]);
            } else if (g == segments) {
                for (s = LOGO_SIDES - 1; s >= 0; s--)
                    glVertex3d(pRingPts[3*s], pRingPts[3*s + 1], pRingPts[3*s + 2]);
            }
            glEnd();

            /* switch pointers to store previous ring */
            tmp = pPrevRing;
            pPrevRing = pRingPts;
            pRingPts = tmp;
            tmp = pPrevNorm;
            pPrevNorm = pRingNorm;
            pRingNorm = tmp;
        }
    }

    glEndList();
}

void OpenGLRenderer::Construct(void)
{
}

END_SCOPE(Cn3D)
