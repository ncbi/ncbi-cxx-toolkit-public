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
* Revision 1.11  2000/08/04 22:49:03  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.10  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.9  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.8  2000/07/18 16:50:11  thiessen
* more friendly rotation center setting
*
* Revision 1.7  2000/07/18 02:41:33  thiessen
* fix bug in virtual bonds and altConfs
*
* Revision 1.6  2000/07/18 00:06:00  thiessen
* allow arbitrary rotation center
*
* Revision 1.5  2000/07/17 22:37:18  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.4  2000/07/17 04:20:49  thiessen
* now does correct structure alignment transformation
*
* Revision 1.3  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/12 23:27:49  thiessen
* now draws basic CPK model
*
* Revision 1.1  2000/07/12 14:11:30  thiessen
* added initial OpenGL rendering engine
*
* ===========================================================================
*/

#if defined(WIN32)
#include <windows.h>

#elif defined(macintosh)
#include <agl.h>

#elif defined(WIN_MOTIF)
#include <GL/glx.h>

#endif

#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

#include "cn3d/opengl_renderer.hpp"
#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

static const double PI = acos(-1);
static inline double DegreesToRad(double deg) { return deg*PI/180.0; }
static inline double RadToDegrees(double rad) { return rad*180.0/PI; }

static const GLuint FIRST_LIST = 1;


// it's easier to keep one global qobj for now
static GLUquadricObj *qobj = NULL;

// pick buffer
const unsigned int OpenGLRenderer::NO_NAME = 0;
static const int pickBufSize = 1024;
static GLuint selectBuf[pickBufSize];

/* these are used for both matrial colors and light colors */
static const GLfloat Color_Off[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static const GLfloat Color_MostlyOff[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
static const GLfloat Color_MostlyOn[4] = { 0.95f, 0.95f, 0.95f, 1.0f };
static const GLfloat Color_On[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

// matrix conversion utility functions

// convert from Matrix to GL-matrix ordering
static void Matrix2GL(const Matrix& m, GLdouble *g)
{
    g[0]=m.m[0];  g[4]=m.m[1];  g[8]=m.m[2];   g[12]=m.m[3];
    g[1]=m.m[4];  g[5]=m.m[5];  g[9]=m.m[6];   g[13]=m.m[7];
    g[2]=m.m[8];  g[6]=m.m[9];  g[10]=m.m[10]; g[14]=m.m[11];
    g[3]=m.m[12]; g[7]=m.m[13]; g[11]=m.m[14]; g[15]=m.m[15];
}

// convert from GL-matrix to Matrix ordering
static void GL2Matrix(GLdouble *g, Matrix *m)
{
    m->m[0]=g[0];  m->m[1]=g[4];  m->m[2]=g[8];   m->m[3]=g[12];
    m->m[4]=g[1];  m->m[5]=g[5];  m->m[6]=g[9];   m->m[7]=g[13];
    m->m[8]=g[2];  m->m[9]=g[6];  m->m[10]=g[10]; m->m[11]=g[14];
    m->m[12]=g[3]; m->m[13]=g[7]; m->m[14]=g[11]; m->m[15]=g[15];
}

// OpenGLRenderer methods - initialization and setup

OpenGLRenderer::OpenGLRenderer(void) : selectMode(false)
{
    // make sure a name will fit in a GLuint
    if (sizeof(GLuint) < sizeof(unsigned int))
        ERR_POST(Fatal << "Cn3D requires that sizeof(GLuint) >= sizeof(unsigned int)");
    
    if (!qobj) { // initialize global qobj
        qobj = gluNewQuadric();
        if (!qobj) ERR_POST(Fatal << "unable to allocate GLUQuadricObj");
        gluQuadricDrawStyle(qobj, GLU_FILL);
        gluQuadricNormals(qobj, GLU_SMOOTH);
        gluQuadricOrientation(qobj, GLU_OUTSIDE);
    }
   
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

    // clear these material colors
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, Color_Off);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, Color_Off);

    // turn on culling to speed rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // misc options
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_NORMALIZE);
}


// methods dealing with the view

void OpenGLRenderer::NewView(int selectX, int selectY) const
{
    GLint Viewport[4];
    glGetIntegerv(GL_VIEWPORT, Viewport);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (selectMode) {
        gluPickMatrix(static_cast<GLdouble>(selectX),
                      static_cast<GLdouble>(Viewport[3] - selectY),
                      1.0, 1.0, Viewport);
    }

    GLdouble aspect = (static_cast<GLdouble>(Viewport[2])) / Viewport[3];
    gluPerspective(RadToDegrees(cameraAngleRad),    // viewing angle (degrees)
                   aspect,                          // w/h aspect
                   cameraClipNear,                  // near clipping plane
                   cameraClipFar);                  // far clipping plane 
    gluLookAt(0.0,0.0,cameraDistance,               // the camera position
              cameraLookAtX,                        // the "look-at" point
              cameraLookAtY,
              0.0,
              0.0,1.0,0.0);                         // the up direction

    glMatrixMode(GL_MODELVIEW);
}

void OpenGLRenderer::ResetCamera(void)
{
    glLoadIdentity();
    rotateSpeed = 0.5;

    // set up initial camera
    cameraLookAtX = cameraLookAtY = 0.0;
    if (structureSet) { // for structure
        cameraAngleRad = DegreesToRad(35.0);
        // calculate camera distance so that structure fits exactly in
        // the window in any rotation (based on structureSet's maxDistFromCenter)
        GLint Viewport[4];
        glGetIntegerv(GL_VIEWPORT, Viewport);
        double angle = cameraAngleRad, aspect = (static_cast<double>(Viewport[2])) / Viewport[3];
        if (aspect < 1.0) angle *= aspect;
        cameraDistance = structureSet->maxDistFromCenter / sin(angle/2);
        cameraClipNear = cameraDistance - structureSet->maxDistFromCenter;
        cameraClipFar = cameraDistance + structureSet->maxDistFromCenter;
        // move structureSet's center to origin
        glTranslated(-structureSet->center.x, -structureSet->center.y, -structureSet->center.z);
        structureSet->rotationCenter = structureSet->center;

    } else { // for logo
        cameraAngleRad = PI / 14;
        cameraDistance = 200;
        cameraClipNear = 0.5*cameraDistance;
        cameraClipFar = 1.5*cameraDistance;
    }

    glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix);
    NewView();
}

void OpenGLRenderer::ChangeView(eViewAdjust control, int dX, int dY, int X2, int Y2)
{
    bool doTranslation = false;
    Vector rotCenter;

    // find out where rotation center is in current GL coordinates
    if (structureSet && (control==eXYRotateHV || control==eZRotateH) && 
        structureSet->rotationCenter != structureSet->center) {
        Matrix m;
        GL2Matrix(viewMatrix, &m);
        rotCenter = structureSet->rotationCenter;
        ApplyTransformation(&rotCenter, m);
        doTranslation = true;
    }

    glLoadIdentity();

    // rotate relative to rotationCenter
    if (doTranslation) glTranslated(rotCenter.x, rotCenter.y, rotCenter.z);

    double pixelSize = 0.05; // estimate for now, will do correctly later...

    switch (control) {
    case eXYRotateHV:
        glRotated(rotateSpeed*dY, 1.0, 0.0, 0.0);
        glRotated(rotateSpeed*dX, 0.0, 1.0, 0.0);
        break;
    case eZRotateH:
        glRotated(rotateSpeed*dX, 0.0, 0.0, 1.0);
        break;
    case eXYTranslateHV:
        glTranslated(dX*pixelSize, dY*pixelSize, 0.0);
        break;
    case eZoomHHVV:
        break;
    case eZoomOut:
        cameraAngleRad *= 1.5;
        NewView();
        break;
    case eZoomIn:
        cameraAngleRad /= 1.5;
        NewView();
        break;
    case eCenterCamera:
        cameraLookAtX = cameraLookAtY = 0.0;
        NewView();
        break;
    }

    if (doTranslation) glTranslated(-rotCenter.x, -rotCenter.y, -rotCenter.z);

    glMultMatrixd(viewMatrix);
    glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix);
}

void OpenGLRenderer::SetSize(GLint width, GLint height) const
{
    glViewport(0, 0, width, height);
    NewView();
}


// methods dealing with structure data and drawing

void OpenGLRenderer::Display(void) const
{
    glClearColor(background[0], background[1], background[2], 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    glMultMatrixd(viewMatrix);

    if (selectMode) {
        glInitNames();
        glPushName(0);
    }
    
    if (structureSet) {
        glCallList(FIRST_LIST);
    } else {
        glCallList(FIRST_LIST); // draw logo
    }
    glFlush();
}

// process selection; return gl-name of result
bool OpenGLRenderer::GetSelected(int x, int y, unsigned int *name)
{
    // render with GL_SELECT mode, to fill selection buffer
    glSelectBuffer(pickBufSize, selectBuf);
    glRenderMode(GL_SELECT);
    selectMode = true;
    NewView(x, y);
    Display();
    GLint hits = glRenderMode(GL_RENDER);
    selectMode = false;

    // parse selection buffer to find name of selected item
    int i, j, p=0, n, top=0;
    GLuint minZ=0;
    *name = NO_NAME;
    for (i=0; i<hits; i++) {
        n = selectBuf[p++];                 // # names
        if (i==0 || minZ > selectBuf[p]) {  // find item with min depth
            minZ = selectBuf[p];
            top = 1;
        } else
            top = 0;
        p++;
        p++;                                // skip max depth
        for (j=0; j<n; j++) {               // loop through n names
            switch (j) {
                case 0: 
                    if (top) *name = static_cast<unsigned int>(selectBuf[p]);
                    break;
                default: 
                    ERR_POST(Warning << "GL select: Got more than 1 name!");
            }
            p++;
        }
    }

    NewView();
    Display();
    if (*name != NO_NAME)
        return true;
    else
        return false;
}

void OpenGLRenderer::AttachStructureSet(StructureSet *targetStructureSet)
{
    structureSet = targetStructureSet;
    if (structureSet) {
        structureSet->renderer = this;
        structureSet->SetCenter();
    }

    // make sure general GL stuff is set up
    background[0] = background[1] = background[2] = 0.0;
    Init();
    ResetCamera();
    Construct();
}

void OpenGLRenderer::Construct(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    SetColor(GL_NONE, 0, 0, 0); // reset color caches in SetColor
    if (structureSet) {
        glNewList(FIRST_LIST, GL_COMPILE);
        structureSet->DrawAll();
        glEndList();
    } else {
        ConstructLogo();
    }
}

// set current GL color; don't change color if it's same as what's current
void OpenGLRenderer::SetColor(int type, float red, float green, float blue, float alpha)
{
    static GLfloat pr, pg, pb, pa;
    static GLenum pt = GL_NONE;

    if (type == GL_NONE) {
        pt = GL_NONE;
        return;
    }

//#ifdef _DEBUG
    if (red == 0.0 && green == 0.0 && blue == 0.0)
        ERR_POST(Warning << "SetColor request color (0,0,0)");
    if (alpha == 0.0)
        ERR_POST(Warning << "SetColor request alpha 0.0");
//#endif

    if (red != pr || green != pg || blue != pb || type != pt || alpha != pa) {
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
        GLfloat rgba[4] = { red, green, blue, alpha };
        glMaterialfv(GL_FRONT_AND_BACK, type, rgba);
        if (type == GL_AMBIENT) {
            /* this is necessary so that fonts are rendered in correct
                color in SGI's OpenGL implementation, and maybe others */
            glColor4fv(rgba);
        }
        pr = red;
        pg = green;
        pb = blue;
        pa = alpha;
    }
}

/* create display list with logo */
void OpenGLRenderer::ConstructLogo(void)
{
    static const GLfloat logoColor[3] = { 100.0f/255, 240.0f/255, 150.0f/255 };
    static const int LOGO_SIDES = 36, segments = 180;
    int i, n, s, g;
    GLdouble bigRad = 12.0, height = 24.0,
        minRad = 0.1, maxRad = 2.0,
        ringPts[LOGO_SIDES * 3], *pRingPts = ringPts,
        prevRing[LOGO_SIDES * 3], *pPrevRing = prevRing, *tmp,
        ringNorm[LOGO_SIDES * 3], *pRingNorm = ringNorm,
        prevNorm[LOGO_SIDES * 3], *pPrevNorm = prevNorm,
        length, startRad, midRad, phase, currentRad, CR[3], H[3], V[3];

    glNewList(FIRST_LIST, GL_COMPILE);

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

void OpenGLRenderer::PushMatrix(const Matrix* m) const
{
    glPushMatrix();
    if (m) {
        GLdouble g[16];
        Matrix2GL(*m, g);
        glMultMatrixd(g);
    }
}

void OpenGLRenderer::PopMatrix(void) const
{
    glPopMatrix();
}

void OpenGLRenderer::DrawAtom(const Vector& site, const AtomStyle& atomStyle)
{
    if (atomStyle.radius <= 0.0) return;
    SetColor(GL_DIFFUSE, atomStyle.color[0], atomStyle.color[1], atomStyle.color[2]);
    glLoadName(static_cast<GLuint>(atomStyle.name));
    glPushMatrix();
    glTranslated(site.x, site.y, site.z);
    gluSphere(qobj, atomStyle.radius, atomStyle.slices, atomStyle.stacks);
    glPopMatrix();
}

static void DoCylinderPlacementTransform(const Vector& a, const Vector& b, double length)
{
    /* to translate into place */
    glTranslated(a.x, a.y, a.z);

    /* to rotate from initial position, so bond points right direction;
       handle special case where both ends share ~same x,y */
    if (fabs(a.y - b.y) < 0.000001 && fabs(a.x - b.x) < 0.000001) {
        if (b.z - a.z < 0.0) glRotated(180.0, 1.0, 0.0, 0.0);
    } else {
        glRotated(RadToDegrees(acos((b.z - a.z) / length)),
                  a.y - b.y, b.x - a.x, 0.0);
    }
}

static void DrawHalfBond(const Vector& site1, const Vector& site2,
    StyleManager::eDisplayStyle style, double radius, 
    bool cap1, bool cap2, int sides, int segments)
{
    // straight line bond
    if (style == StyleManager::eLineBond || (style == StyleManager::eCylinderBond && radius <= 0.0)) {
        glBegin(GL_LINES);
        glVertex3d(site1.x, site1.y, site1.z);
        glVertex3d(site2.x, site2.y, site2.z);
        glEnd();
    }

    // cylinder bond
    else if (style == StyleManager::eCylinderBond) {
        double length = (site1 - site2).length();
        if (length <= 0.000001 || sides <= 0) return;
        glPushMatrix();
        DoCylinderPlacementTransform(site1, site2, length);
        gluCylinder(qobj, radius, radius, length, sides, 1);
        if (cap1) {
            glPushMatrix();
            glRotated(180.0, 0.0, 1.0, 0.0);
            gluDisk(qobj, 0.0, radius, sides, 1);
            glPopMatrix();
        }
        if (cap2) {
            glPushMatrix();
            glTranslated(0.0, 0.0, length);
            gluDisk(qobj, 0.0, radius, sides, 1);
            glPopMatrix();
        }
        glPopMatrix();
    }

    // worm bond
    else if (style == StyleManager::eWormBond) {
    }
}

void OpenGLRenderer::DrawBond(const Vector& site1, const Vector& site2, const BondStyle& style)
{
    Vector midpoint = (site1 + site2) / 2;

    GLenum colorType =
        (style.end1.style == StyleManager::eLineBond || style.end1.radius <= 0.0)
        ? GL_AMBIENT : GL_DIFFUSE;
    SetColor(colorType, style.end1.color[0], style.end1.color[1], style.end1.color[2]);
    glLoadName(static_cast<GLuint>(style.end1.name));
    DrawHalfBond(site1, midpoint, style.end1.style, style.end1.radius,
        style.end1.atomCap, style.midCap, style.end1.sides, style.end1.segments);

    colorType =
        (style.end2.style == StyleManager::eLineBond || style.end2.radius <= 0.0)
        ? GL_AMBIENT : GL_DIFFUSE;
    SetColor(colorType, style.end2.color[0], style.end2.color[1], style.end2.color[2]);
    glLoadName(static_cast<GLuint>(style.end2.name));
    DrawHalfBond(midpoint, site2, style.end2.style, style.end2.radius,
        style.midCap, style.end2.atomCap, style.end2.sides, style.end2.segments);
}

END_SCOPE(Cn3D)
