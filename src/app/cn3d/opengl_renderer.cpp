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
* Revision 1.17  2000/08/17 14:24:06  thiessen
* added working StyleManager
*
* Revision 1.16  2000/08/16 14:18:46  thiessen
* map 3-d objects to molecules
*
* Revision 1.15  2000/08/13 02:43:01  thiessen
* added helix and strand objects
*
* Revision 1.14  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.13  2000/08/07 14:13:15  thiessen
* added animation frames
*
* Revision 1.12  2000/08/07 00:21:17  thiessen
* add display list mechanism
*
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
#include "cn3d/style_manager.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

static const double PI = acos(-1);
static inline double DegreesToRad(double deg) { return deg*PI/180.0; }
static inline double RadToDegrees(double rad) { return rad*180.0/PI; }

const unsigned int OpenGLRenderer::NO_LIST = 0;
const unsigned int OpenGLRenderer::FIRST_LIST = 1;

static const unsigned int ALL_FRAMES = 4294967295;

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
   
    // will eventually load these from registry...
    atomSlices = 7;
    atomStacks = 4;
    bondSides = 6;
    wormSides = 6;
    wormSegments = 3;
    helixSides = 12;

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

void OpenGLRenderer::SetSize(int width, int height) const
{
    glViewport(0, 0, width, height);
    TESTMSG("viewport " << width << ' ' << height);
    NewView();
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

// display list management stuff
void OpenGLRenderer::StartDisplayList(unsigned int list)
{
    SetColor(GL_NONE, 0, 0, 0); // reset color caches in SetColor
    TESTMSG("creating display list " << list);
    glNewList(list, GL_COMPILE);
}

void OpenGLRenderer::EndDisplayList(void)
{
    glEndList();
}


// frame management methods

void OpenGLRenderer::ShowAllFrames(void)
{
    if (structureSet) currentFrame = ALL_FRAMES;
}

void OpenGLRenderer::ShowFirstFrame(void)
{
    if (structureSet) currentFrame = 0;
}

void OpenGLRenderer::ShowLastFrame(void)
{
    if (structureSet) currentFrame = structureSet->frameMap.size() - 1;
}

void OpenGLRenderer::ShowNextFrame(void)
{
    if (structureSet) {
        if (currentFrame >= structureSet->frameMap.size() - 1)
            currentFrame = 0;
        else
            currentFrame++;
    }
}

void OpenGLRenderer::ShowPreviousFrame(void)
{
    if (structureSet) {
        if (currentFrame == 0 || currentFrame == ALL_FRAMES)
            currentFrame = structureSet->frameMap.size() - 1;
        else
            currentFrame--;
    }
}


// methods dealing with structure data and drawing

void OpenGLRenderer::Display(void) const
{
    if (structureSet) {
        const Vector& background = structureSet->styleManager->GetBackgroundColor();
        glClearColor(background[0], background[1], background[2], 1.0);
    } else
        glClearColor(0.0, 0.0, 0.0, 1.0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glMultMatrixd(viewMatrix);

    if (selectMode) {
        glInitNames();
        glPushName(0);
    }
    
    if (structureSet) {
        if (currentFrame == ALL_FRAMES) {
            for (unsigned int i=FIRST_LIST; i<=structureSet->lastDisplayList; i++)
                glCallList(i);
        } else {
            StructureSet::DisplayLists::const_iterator
                l, le=structureSet->frameMap[currentFrame].end();
            for (l=structureSet->frameMap[currentFrame].begin(); l!=le; l++)
                glCallList(*l);
        }
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
    NewView();

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
    Init();
    ResetCamera();
    Construct();
    currentFrame = ALL_FRAMES;
}

void OpenGLRenderer::Construct(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (structureSet) {
        structureSet->DrawAll();
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

void OpenGLRenderer::DrawAtom(const Vector& site, const AtomStyle& atomStyle)
{
    if (atomStyle.radius <= 0.0) return;
    SetColor(GL_DIFFUSE, atomStyle.color[0], atomStyle.color[1], atomStyle.color[2]);
    glLoadName(static_cast<GLuint>(atomStyle.name));
    glPushMatrix();
    glTranslated(site.x, site.y, site.z);
    gluSphere(qobj, atomStyle.radius, atomSlices, atomStacks);
    glPopMatrix();
}

/* add a thick splined curve from point 1 *halfway* to point 2 */
static void DrawHalfWorm(const Vector *p0, const Vector& p1,
    const Vector& p2, const Vector *p3,
    double radius, bool cap1, bool cap2,
    double tension, int sides, int segments)
{
    int i, j, k, m, offset;
    Vector R1, R2, Qt, p, dQt, H, V;
    double len, MG[4][3], T[4], t, prevlen, cosj, sinj;
    GLdouble *Nx = NULL, *Ny, *Nz, *Cx, *Cy, *Cz,
        *pNx, *pNy, *pNz, *pCx, *pCy, *pCz, *tmp;

    if (!p0 || !p3) {
        ERR_POST(Error << "DrawHalfWorm: got NULL 0th and/or 3rd coords for worm");
        return;
    }

    /*
     * The Hermite matrix Mh.
     */
    static double Mh[4][4] = {
        { 2, -2,  1,  1},
        {-3,  3, -2, -1},
        { 0,  0,  1,  0},
        { 1,  0,  0,  0}
    };

    /*
     * Variables that affect the curve shape
     *   a=b=0 = Catmull-Rom
     */
    double a = tension,         /* tension    (adjustable)  */
        c = 0,                  /* continuity (should be 0) */
        b = 0;                  /* bias       (should be 0) */

    if (sides % 2) {
        ERR_POST(Warning << "worm sides must be an even number");
        sides++;
    }
    GLdouble *fblock = new GLdouble[12 * sides];

    /* First, calculate the coordinate points of the center of the worm,
     * using the Kochanek-Bartels variant of the Hermite curve.
     */
    R1 = 0.5 * (1 - a) * (1 + b) * (1 + c) * (p1 - *p0) + 0.5 * (1 - a) * (1 - b) * (1 - c) * ( p2 - p1);
    R2 = 0.5 * (1 - a) * (1 + b) * (1 - c) * (p2 -  p1) + 0.5 * (1 - a) * (1 - b) * (1 + c) * (*p3 - p2);

    /*
     * Multiply MG=Mh.Gh, where Gh = [ P(1) P(2) R(1) R(2) ]. This
     * 4x1 matrix of vectors is constant for each segment.
     */
    for (i = 0; i < 4; i++) {   /* calculate Mh.Gh */
        MG[i][0] = Mh[i][0] * p1.x + Mh[i][1] * p2.x + Mh[i][2] * R1.x + Mh[i][3] * R2.x;
        MG[i][1] = Mh[i][0] * p1.y + Mh[i][1] * p2.y + Mh[i][2] * R1.y + Mh[i][3] * R2.y;
        MG[i][2] = Mh[i][0] * p1.z + Mh[i][1] * p2.z + Mh[i][2] * R1.z + Mh[i][3] * R2.z;
    }

    for (i = 0; i <= segments; i++) {

        /* t goes from [0,1] from P(1) to P(2) (and we want to go halfway only),
           and the function Q(t) defines the curve of this segment. */
        t = (0.5 / segments) * i;
        /*
           * Q(t)=T.(Mh.Gh), where T = [ t^3 t^2 t 1 ]
         */
        T[0] = t * t * t;
        T[1] = t * t;
        T[2] = t;
        //T[3] = 1;
        Qt.x = T[0] * MG[0][0] + T[1] * MG[1][0] + T[2] * MG[2][0] + MG[3][0] /* *T[3] */ ;
        Qt.y = T[0] * MG[0][1] + T[1] * MG[1][1] + T[2] * MG[2][1] + MG[3][1] /* *T[3] */ ;
        Qt.z = T[0] * MG[0][2] + T[1] * MG[1][2] + T[2] * MG[2][2] + MG[3][2] /* *T[3] */ ;

        if (radius == 0.0) {
            if (i > 0) {
                glBegin(GL_LINES);
                glVertex3d(p.x, p.y, p.z);
                glVertex3d(Qt.x, Qt.y, Qt.z);
                glEnd();
            }
            /* save to use as previous point for connecting points together */
            p = Qt;

        } else {
            /* construct a circle of points centered at and
               in a plane normal to the curve at t - these points will
               be used to construct the "thick" worm */

            /* allocate single block of storage for two circles of points */
            if (!Nx) {
                Nx = fblock;
                Ny = &Nx[sides];
                Nz = &Nx[sides * 2];
                Cx = &Nx[sides * 3];
                Cy = &Nx[sides * 4];
                Cz = &Nx[sides * 5];
                pNx = &Nx[sides * 6];
                pNy = &Nx[sides * 7];
                pNz = &Nx[sides * 8];
                pCx = &Nx[sides * 9];
                pCy = &Nx[sides * 10];
                pCz = &Nx[sides * 11];
            }

            /*
             * The first derivative of Q(t), d(Q(t))/dt, is the slope
             * (tangent) at point Q(t); now T = [ 3t^2 2t 1 0 ]
             */
            T[0] = t * t * 3;
            T[1] = t * 2;
            //T[2] = 1;
            //T[3] = 0;
            dQt.x = T[0] * MG[0][0] + T[1] * MG[1][0] + MG[2][0] /* *T[2] + T[3]*MG[3][0] */ ;
            dQt.y = T[0] * MG[0][1] + T[1] * MG[1][1] + MG[2][1] /* *T[2] + T[3]*MG[3][1] */ ;
            dQt.z = T[0] * MG[0][2] + T[1] * MG[1][2] + MG[2][2] /* *T[2] + T[3]*MG[3][2] */ ;

            /* use cross prod't of [1,0,0] x normal as horizontal */
            H = Vector(0.0, -dQt.z, dQt.y);
            if (H.length() < 0.000001) /* nearly colinear - use [1,0.1,0] instead */
                H = Vector(0.1 * dQt.z, -dQt.z, dQt.y - 0.1 * dQt.x);
            H.normalize();

            /* and a vertical vector = normal x H */
            V = vector_cross(dQt, H);
            V.normalize();

            /* finally, the worm circumference points (C) and normals (N) are
               simple trigonomic combinations of H and V */
            for (j = 0; j < sides; j++) {
                cosj = cos(2 * PI * j / sides);
                sinj = sin(2 * PI * j / sides);
                Nx[j] = H.x * cosj + V.x * sinj;
                Ny[j] = H.y * cosj + V.y * sinj;
                Nz[j] = H.z * cosj + V.z * sinj;
                Cx[j] = Qt.x + Nx[j] * radius;
                Cy[j] = Qt.y + Ny[j] * radius;
                Cz[j] = Qt.z + Nz[j] * radius;
            }

            /* figure out which points on the previous circle "match" best
               with these, to minimize envelope twisting */
            for (m = 0; m < sides; m++) {
                len = 0.0;
                for (j = 0; j < sides; j++) {
                    k = j + m;
                    if (k >= sides)
                        k -= sides;
                    len += (Cx[k] - pCx[j]) * (Cx[k] - pCx[j]) +
                           (Cy[k] - pCy[j]) * (Cy[k] - pCy[j]) +
                           (Cz[k] - pCz[j]) * (Cz[k] - pCz[j]);
                }
                if (m == 0 || len < prevlen) {
                    prevlen = len;
                    offset = m;
                }
            }

            /* create triangles from points along this and previous circle */
            if (i > 0) {
                glBegin(GL_TRIANGLE_STRIP);
                for (j = 0; j < sides; j++) {
                    k = j + offset;
                    if (k >= sides) k -= sides;
                    glNormal3d(Nx[k], Ny[k], Nz[k]);
                    glVertex3d(Cx[k], Cy[k], Cz[k]);
                    glNormal3d(pNx[j], pNy[j], pNz[j]);
                    glVertex3d(pCx[j], pCy[j], pCz[j]);
                }
                glNormal3d(Nx[offset], Ny[offset], Nz[offset]);
                glVertex3d(Cx[offset], Cy[offset], Cz[offset]);
                glNormal3d(pNx[0], pNy[0], pNz[0]);
                glVertex3d(pCx[0], pCy[0], pCz[0]);
                glEnd();
            }

            /* put caps on the end */
            if (cap1 && i == 0) {
                glBegin(GL_POLYGON);
                dQt.normalize();
                glNormal3d(-dQt.x, -dQt.y, -dQt.z);
                for (j = sides - 1; j >= 0; j--) {
                    k = j + offset;
                    if (k >= sides) k -= sides;
                    glVertex3d(Cx[k], Cy[k], Cz[k]);
                }
                glEnd();
            }
            else if (cap2 && i == segments) {
                glBegin(GL_POLYGON);
                dQt.normalize();
                glNormal3d(dQt.x, dQt.y, dQt.z);
                for (j = 0; j < sides; j++) {
                    k = j + offset;
                    if (k >= sides) k -= sides;
                    glVertex3d(Cx[k], Cy[k], Cz[k]);
                }
                glEnd();
            }

            /* store this circle as previous for next round; instead of copying
               all values, just swap pointers */
#define SWAPPTR(p1,p2) tmp=(p1); (p1)=(p2); (p2)=tmp
            SWAPPTR(Nx, pNx);
            SWAPPTR(Ny, pNy);
            SWAPPTR(Nz, pNz);
            SWAPPTR(Cx, pCx);
            SWAPPTR(Cy, pCy);
            SWAPPTR(Cz, pCz);
        }
    }

    delete fblock;
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

static void DrawHalfBond(const Vector& site1, const Vector& midpoint,
    StyleManager::eDisplayStyle style, double radius, 
    bool cap1, bool cap2, int bondSides)
{
    // straight line bond
    if (style == StyleManager::eLineBond || (style == StyleManager::eCylinderBond && radius <= 0.0)) {
        glBegin(GL_LINES);
        glVertex3d(site1.x, site1.y, site1.z);
        glVertex3d(midpoint.x, midpoint.y, midpoint.z);
        glEnd();
    }

    // cylinder bond
    else if (style == StyleManager::eCylinderBond) {
        double length = (site1 - midpoint).length();
        if (length <= 0.000001 || bondSides <= 0) return;
        glPushMatrix();
        DoCylinderPlacementTransform(site1, midpoint, length);
        gluCylinder(qobj, radius, radius, length, bondSides, 1);
        if (cap1) {
            glPushMatrix();
            glRotated(180.0, 0.0, 1.0, 0.0);
            gluDisk(qobj, 0.0, radius, bondSides, 1);
            glPopMatrix();
        }
        if (cap2) {
            glPushMatrix();
            glTranslated(0.0, 0.0, length);
            gluDisk(qobj, 0.0, radius, bondSides, 1);
            glPopMatrix();
        }
        glPopMatrix();
    }
}

void OpenGLRenderer::DrawBond(const Vector& site1, const Vector& site2,
    const BondStyle& style, const Vector *site0, const Vector* site3)
{
    Vector midpoint = (site1 + site2) / 2;

    GLenum colorType =
        (style.end1.style == StyleManager::eLineBond || 
         style.end1.style == StyleManager::eLineWormBond || 
         style.end1.radius <= 0.0)
        ? GL_AMBIENT : GL_DIFFUSE;
    SetColor(colorType, style.end1.color[0], style.end1.color[1], style.end1.color[2]);
    glLoadName(static_cast<GLuint>(style.end1.name));
    if (style.end1.style == StyleManager::eLineWormBond || 
         style.end1.style == StyleManager::eThickWormBond) 
        DrawHalfWorm(site0, site1, site2, site3,
            (style.end1.style == StyleManager::eThickWormBond) ? style.end1.radius : 0.0,
            style.end1.atomCap, style.midCap, 
            style.tension, wormSides, wormSegments);
    else
        DrawHalfBond(site1, midpoint,
            style.end1.style, style.end1.radius,
            style.end1.atomCap, style.midCap, bondSides);

    colorType =
        (style.end2.style == StyleManager::eLineBond ||
         style.end2.style == StyleManager::eLineWormBond || 
         style.end2.radius <= 0.0)
        ? GL_AMBIENT : GL_DIFFUSE;
    SetColor(colorType, style.end2.color[0], style.end2.color[1], style.end2.color[2]);
    glLoadName(static_cast<GLuint>(style.end2.name));
    if (style.end2.style == StyleManager::eLineWormBond || 
         style.end2.style == StyleManager::eThickWormBond) 
        DrawHalfWorm(site3, site2, site1, site0,
            (style.end2.style == StyleManager::eThickWormBond) ? style.end2.radius : 0.0,
            style.end2.atomCap, style.midCap, 
            style.tension, wormSides, wormSegments);
    else
        DrawHalfBond(midpoint, site2,
            style.end2.style, style.end2.radius,
            style.midCap, style.end2.atomCap, bondSides);
}

void OpenGLRenderer::DrawHelix(const Vector& Nterm, const Vector& Cterm, const HelixStyle& style)
{
    SetColor(GL_DIFFUSE, style.color[0], style.color[1], style.color[2]);
    glLoadName(static_cast<GLuint>(NO_NAME));

    double length = (Nterm - Cterm).length();
    if (length <= 0.000001) return;

    glPushMatrix();
    DoCylinderPlacementTransform(Nterm, Cterm, length);
    if (style.style == StyleManager::eObjectWithArrow)
        length -= style.arrowLength;
    gluCylinder(qobj, style.radius, style.radius, length, helixSides, 1);
    
    // Nterm cap
    glPushMatrix();
    glRotated(180.0, 0.0, 1.0, 0.0);
    gluDisk(qobj, 0.0, style.radius, helixSides, 1);
    glPopMatrix();

    // Cterm Arrow
    if (style.style == StyleManager::eObjectWithArrow) {
        glPushMatrix();
        glTranslated(0.0, 0.0, length);
        if (style.arrowBaseWidthProportion > 1.0) {
            glPushMatrix();
            glRotated(180.0, 0.0, 1.0, 0.0);
            gluDisk(qobj, 0.0, style.radius * style.arrowBaseWidthProportion, helixSides, 1);
            glPopMatrix();
        }
        gluCylinder(qobj, style.radius * style.arrowBaseWidthProportion,
            style.radius * style.arrowTipWidthProportion, style.arrowLength, helixSides, 10);
        if (style.arrowTipWidthProportion > 0.0) {
            glTranslated(0.0, 0.0, style.arrowLength);
            gluDisk(qobj, 0.0, style.radius * style.arrowTipWidthProportion, helixSides, 1);
        }
        glPopMatrix();

    // Cterm cap
    } else {
        glPushMatrix();
        glTranslated(0.0, 0.0, length);
        gluDisk(qobj, 0.0, style.radius, helixSides, 1);
        glPopMatrix();
    }
   
    glPopMatrix();
}

void OpenGLRenderer::DrawStrand(const Vector& Nterm, const Vector& Cterm, 
    const Vector& unitNormal, const StrandStyle& style)
{
    GLdouble c000[3], c001[3], c010[3], c011[3],
             c100[3], c101[3], c110[3], c111[3], n[3];
    Vector a, h;
    int i;

    SetColor(GL_DIFFUSE, style.color[0], style.color[1], style.color[2]);
    glLoadName(static_cast<GLuint>(NO_NAME));

    /* in this brick's world coordinates, the long axis (N-C direction) is
       along +Z, with N terminus at Z=0; width is in the X direction, and
       thickness in Y. Arrowhead at C-terminus, of course. */
         
    a = Cterm - Nterm;
    a.normalize();
    h = vector_cross(unitNormal, a);

    Vector lCterm(Cterm);
    if (style.style == StyleManager::eObjectWithArrow)
        lCterm -= a * style.arrowLength;
            
    for (i=0; i<3; i++) {
        c000[i] =  Nterm[i] - h[i]*style.width/2 - unitNormal[i]*style.thickness/2;
        c001[i] = lCterm[i] - h[i]*style.width/2 - unitNormal[i]*style.thickness/2;
        c010[i] =  Nterm[i] - h[i]*style.width/2 + unitNormal[i]*style.thickness/2;
        c011[i] = lCterm[i] - h[i]*style.width/2 + unitNormal[i]*style.thickness/2;
        c100[i] =  Nterm[i] + h[i]*style.width/2 - unitNormal[i]*style.thickness/2;
        c101[i] = lCterm[i] + h[i]*style.width/2 - unitNormal[i]*style.thickness/2;
        c110[i] =  Nterm[i] + h[i]*style.width/2 + unitNormal[i]*style.thickness/2;
        c111[i] = lCterm[i] + h[i]*style.width/2 + unitNormal[i]*style.thickness/2;
    }

    glBegin(GL_QUADS);

    for (i=0; i<3; i++) n[i] = unitNormal[i];
    glNormal3dv(n);
    glVertex3dv(c010);
    glVertex3dv(c011);
    glVertex3dv(c111);
    glVertex3dv(c110);

    for (i=0; i<3; i++) n[i] = -unitNormal[i];
    glNormal3dv(n);
    glVertex3dv(c000);
    glVertex3dv(c100);
    glVertex3dv(c101);
    glVertex3dv(c001);

    for (i=0; i<3; i++) n[i] = h[i];
    glNormal3dv(n);
    glVertex3dv(c100);
    glVertex3dv(c110);
    glVertex3dv(c111);
    glVertex3dv(c101);

    for (i=0; i<3; i++) n[i] = -h[i];
    glNormal3dv(n);
    glVertex3dv(c000);
    glVertex3dv(c001);
    glVertex3dv(c011);
    glVertex3dv(c010);

    for (i=0; i<3; i++) n[i] = -a[i];
    glNormal3dv(n);
    glVertex3dv(c000);
    glVertex3dv(c010);
    glVertex3dv(c110);
    glVertex3dv(c100);

    if (style.style == StyleManager::eObjectWithoutArrow) {
        for (i=0; i<3; i++) n[i] = a[i];
        glNormal3dv(n);
        glVertex3dv(c001);
        glVertex3dv(c101);
        glVertex3dv(c111);
        glVertex3dv(c011);

    } else {
        GLdouble FT[3], LT[3], RT[3], FB[3], LB[3], RB[3];
    
        for (i=0; i<3; i++) {
            FT[i] = lCterm[i] + unitNormal[i]*style.thickness/2 + 
                a[i]*style.arrowLength;
            LT[i] = lCterm[i] + unitNormal[i]*style.thickness/2 + 
                h[i]*style.arrowBaseWidthProportion*style.width/2;
            RT[i] = lCterm[i] + unitNormal[i]*style.thickness/2 - 
                h[i]*style.arrowBaseWidthProportion*style.width/2;
            FB[i] = lCterm[i] - unitNormal[i]*style.thickness/2 + 
                a[i]*style.arrowLength;
            LB[i] = lCterm[i] - unitNormal[i]*style.thickness/2 + 
                h[i]*style.arrowBaseWidthProportion*style.width/2;
            RB[i] = lCterm[i] - unitNormal[i]*style.thickness/2 - 
                h[i]*style.arrowBaseWidthProportion*style.width/2;
        }

        // the back-facing rectangles on the base of the arrow
        for (i=0; i<3; i++) n[i] = -a[i];
        glNormal3dv(n);
        glVertex3dv(c111);
        glVertex3dv(LT);
        glVertex3dv(LB);
        glVertex3dv(c101);

        glVertex3dv(c011);
        glVertex3dv(c001);
        glVertex3dv(RB);
        glVertex3dv(RT);

        // the left side of the arrow
        for (i=0; i<3; i++) h[i] = FT[i] - LT[i];
        Vector nL = vector_cross(unitNormal, h);
        nL.normalize();
        for (i=0; i<3; i++) n[i] = nL[i];
        glNormal3dv(n);
        glVertex3dv(FT);
        glVertex3dv(FB);
        glVertex3dv(LB);
        glVertex3dv(LT);

        // the right side of the arrow
        for (i=0; i<3; i++) h[i] = FT[i] - RT[i];
        Vector nR = vector_cross(h, unitNormal);
        nR.normalize();
        for (i=0; i<3; i++) n[i] = nR[i];
        glNormal3dv(n);
        glVertex3dv(FT);
        glVertex3dv(RT);
        glVertex3dv(RB);
        glVertex3dv(FB);

        glEnd();
        glBegin(GL_TRIANGLES);
        
        // the top and bottom arrow triangles
        for (i=0; i<3; i++) n[i] = unitNormal[i];
        glNormal3dv(n);
        glVertex3dv(FT);
        glVertex3dv(LT);
        glVertex3dv(RT);
        
        for (i=0; i<3; i++) n[i] = -unitNormal[i];
        glNormal3dv(n);
        glVertex3dv(FB);
        glVertex3dv(RB);
        glVertex3dv(LB);
    }

    glEnd();
}

END_SCOPE(Cn3D)
