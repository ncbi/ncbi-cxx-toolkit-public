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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp> // avoids some 'CurrentTime' conflict later on...
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.h>

#if defined(__WXMSW__)
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#elif defined(__WXGTK__)
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#elif defined(__WXMAC__)
//#include <Fonts.h>
#include <AGL/agl.h>
//#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#endif

#include <math.h>
#include <stdlib.h> // for rand, srand

#include <objects/cn3d/Cn3d_GL_matrix.hpp>
#include <objects/cn3d/Cn3d_vector.hpp>

#define GL_ENUM_TYPE GLenum
#define GL_INT_TYPE GLint
#define GL_DOUBLE_TYPE GLdouble

#include "remove_header_conflicts.hpp"

#include "opengl_renderer.hpp"
#include "structure_window.hpp"
#include "cn3d_glcanvas.hpp"
#include "structure_set.hpp"
#include "style_manager.hpp"
#include "messenger.hpp"
#include "cn3d_tools.hpp"
#include "cn3d_colors.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// whether to use my (limited) GLU quadric functions, or the native ones
#define USE_MY_GLU_QUADS 1

// enables "uniform data" gl calls - e.g. glMaterial for every vertex, not just when the color changes
#ifdef __WXMAC__

#define MAC_GL_OPTIMIZE 1
#define MAC_GL_SETCOLOR SetColor(eUseCachedValues);
#ifndef USE_MY_GLU_QUADS
#define USE_MY_GLU_QUADS 1    // necessary for mac GL optimization
#endif

#else // !__WXMAC__

#define MAC_GL_SETCOLOR

#endif // __WXMAC__

#ifndef USE_MY_GLU_QUADS
// it's easier to keep one global qobj for now
static GLUquadricObj *qobj = NULL;
#endif

static const double PI = acos(-1.0);
static inline double DegreesToRad(double deg) { return deg*PI/180.0; }
static inline double RadToDegrees(double rad) { return rad*180.0/PI; }

const unsigned int OpenGLRenderer::NO_LIST = 0;
const unsigned int OpenGLRenderer::FIRST_LIST = 1;
const unsigned int OpenGLRenderer::FONT_BASE = 100000;

static const unsigned int ALL_FRAMES = kMax_UInt;

// pick buffer
const unsigned int OpenGLRenderer::NO_NAME = 0;
static const int pickBufSize = 1024;
static GLuint selectBuf[pickBufSize];

/* these are used for both matrial colors and light colors */
static const GLfloat Color_Off[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static const GLfloat Color_MostlyOff[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
static const GLfloat Color_MostlyOn[4] = { 0.95f, 0.95f, 0.95f, 1.0f };
static const GLfloat Color_On[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static const GLfloat Color_Specular[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
static const GLint Shininess = 40;

// to cache registry values
static int atomSlices, atomStacks, bondSides, wormSides, wormSegments, helixSides;
static bool highlightsOn;
static string projectionType;


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

// my (limited) replacements for glu functions - these only do solid smooth objects
#if USE_MY_GLU_QUADS

static const GLdouble origin[] = { 0.0, 0.0, 0.0 }, unitZ[] = { 0.0, 0.0, 1.0 };

#define GLU_DISK(q, i, o, s, l) MyGluDisk((i), (o), (s), (l))

void OpenGLRenderer::MyGluDisk(GLdouble innerRadius, GLdouble outerRadius, GLint slices, GLint loops)
{
    if (slices < 3 || loops < 1 || innerRadius < 0.0 || innerRadius >= outerRadius) {
        ERRORMSG("MyGluDisk() - bad parameters");
        return;
    }

    // calculate all the x,y coordinates (at radius 1)
    vector < GLdouble > x(slices), y(slices);
    int l = 0, s, i;
    GLdouble f, f2, a;
    for (s=0; s<slices; ++s) {
        a = PI * 2 * s / slices;
        x[s] = cos(a);
        y[s] = sin(a);
    }

    // if innerRadius is zero, then make the center a triangle fan
    if (innerRadius == 0.0) {
        f = innerRadius + (outerRadius - innerRadius) / loops;
        glBegin(GL_TRIANGLE_FAN);
        MAC_GL_SETCOLOR
        glNormal3dv(unitZ);
        glVertex3dv(origin);
        for (s=0; s<=slices; ++s) {
            i = (s == slices) ? 0 : s;
            MAC_GL_SETCOLOR
            glVertex3d(x[i] * f, y[i] * f, 0.0);
        }
        glEnd();
        ++l;
    }

    // outer loops (or all if innerRadius > zero) get quad strips
    for (; l<loops; ++l) {
        f = innerRadius + (outerRadius - innerRadius) * l / loops;
        f2 = innerRadius + (outerRadius - innerRadius) * (l + 1) / loops;
        glBegin(GL_QUAD_STRIP);
        glNormal3dv(unitZ);
        for (s=0; s<=slices; ++s) {
            i = (s == slices) ? 0 : s;
            MAC_GL_SETCOLOR
            glVertex3d(x[i] * f, y[i] * f, 0.0);
            MAC_GL_SETCOLOR
            glVertex3d(x[i] * f2, y[i] * f2, 0.0);
        }
        glEnd();
    }
}

#define GLU_CYLINDER(q, b, t, h, l, k) MyGluCylinder((b), (t), (h), (l), (k))

void OpenGLRenderer::MyGluCylinder(GLdouble baseRadius, GLdouble topRadius, GLdouble height, GLint slices, GLint stacks)
{
    if (slices < 3 || stacks < 1 || height <= 0.0 || baseRadius < 0.0 || topRadius < 0.0 ||
            (baseRadius == 0.0 && topRadius == 0.0)) {
        ERRORMSG("MyGluCylinder() - bad parameters");
        return;
    }

    // calculate all the x,y coordinates (at radius 1)
    vector < GLdouble > x(slices), y(slices);
    vector < Vector > N(slices);
    int k, s, i;
    GLdouble f, f2, a;
    Matrix r;
    for (s=0; s<slices; ++s) {
        a = PI * 2 * s / slices;
        x[s] = cos(a);
        y[s] = sin(a);
        a += PI / 2;
        SetRotationMatrix(&r, Vector(cos(a), sin(a), 0.0), atan((topRadius - baseRadius) / height));
        N[s].Set(x[s], y[s], 0.0);
        ApplyTransformation(&(N[s]), r);
    }

    // create each stack out of a quad strip
    for (k=0; k<stacks; ++k) {
        f = baseRadius + (topRadius - baseRadius) * k / stacks;
        f2 = baseRadius + (topRadius - baseRadius) * (k + 1) / stacks;
        glBegin(GL_QUAD_STRIP);
        for (s=0; s<=slices; ++s) {
            i = (s == slices) ? 0 : s;
            MAC_GL_SETCOLOR
            glNormal3d(N[i].x, N[i].y, N[i].z);
            glVertex3d(x[i] * f2, y[i] * f2, height * (k + 1) / stacks);
            MAC_GL_SETCOLOR
            glVertex3d(x[i] * f, y[i] * f, height * k / stacks);
        }
        glEnd();
    }
}

#define GLU_SPHERE(q, r, l, k) MyGluSphere((r), (l), (k))

void OpenGLRenderer::MyGluSphere(GLdouble radius, GLint slices, GLint stacks)
{
    if (slices < 3 || stacks < 2 || radius <= 0.0) {
        ERRORMSG("MyGluSphere() - bad parameters");
        return;
    }

    // calculate all the x,y coordinates (at radius 1)
    vector < vector < Vector > > N(stacks - 1);
    int k, s, i;
    GLdouble z, a, r;
    for (k=0; k<stacks-1; ++k) {
        N[k].resize(slices);
        a = PI * (-0.5 + (1.0 + k) / stacks);
        z = sin(a);
        r = cos(a);
        for (s=0; s<slices; ++s) {
            a = PI * 2 * s / slices;
            N[k][s].Set(cos(a) * r, sin(a) * r, z);
        }
    }

    // bottom triangle fan
    glBegin(GL_TRIANGLE_FAN);
    MAC_GL_SETCOLOR
    glNormal3d(0.0, 0.0, -1.0);
    glVertex3d(0.0, 0.0, -radius);
    for (s=slices; s>=0; --s) {
        i = (s == slices) ? 0 : s;
        const Vector& n = N[0][i];
        MAC_GL_SETCOLOR
        glNormal3d(n.x, n.y, n.z);
        glVertex3d(n.x * radius, n.y * radius, n.z * radius);
    }
    glEnd();

    // middle quad strips
    for (k=0; k<stacks-2; ++k) {
        glBegin(GL_QUAD_STRIP);
        for (s=slices; s>=0; --s) {
            i = (s == slices) ? 0 : s;
            const Vector& n1 = N[k][i];
            MAC_GL_SETCOLOR
            glNormal3d(n1.x, n1.y, n1.z);
            glVertex3d(n1.x * radius, n1.y * radius, n1.z * radius);
            const Vector& n2 = N[k + 1][i];
            MAC_GL_SETCOLOR
            glNormal3d(n2.x, n2.y, n2.z);
            glVertex3d(n2.x * radius, n2.y * radius, n2.z * radius);
        }
        glEnd();
    }

    // top triangle fan
    glBegin(GL_TRIANGLE_FAN);
    MAC_GL_SETCOLOR
    glNormal3dv(unitZ);
    glVertex3d(0.0, 0.0, radius);
    for (s=0; s<=slices; ++s) {
        i = (s == slices) ? 0 : s;
        const Vector& n = N[stacks - 2][i];
        MAC_GL_SETCOLOR
        glNormal3d(n.x, n.y, n.z);
        glVertex3d(n.x * radius, n.y * radius, n.z * radius);
    }
    glEnd();
}

#else   // !USE_MY_GLU_QUADS

#define GLU_DISK gluDisk
#define GLU_CYLINDER gluCylinder
#define GLU_SPHERE gluSphere

#endif  // USE_MY_GLU_QUADS

// OpenGLRenderer methods - initialization and setup

OpenGLRenderer::OpenGLRenderer(Cn3DGLCanvas *parentGLCanvas) :
    structureSet(NULL), glCanvas(parentGLCanvas),
    cameraAngleRad(0.0), rotateSpeed(0.5), selectMode(false), currentDisplayList(NO_LIST),
    stereoOn(false)
{
    // make sure a name will fit in a GLuint
    if (sizeof(GLuint) < sizeof(unsigned int))
        FATALMSG("Cn3D requires that sizeof(GLuint) >= sizeof(unsigned int)");
}

void OpenGLRenderer::Init(void) const
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // set up the lighting
    // directional light (faster) when LightPosition[4] == 0.0
    GLfloat LightPosition[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, LightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, Color_Off);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, Color_MostlyOn);
    glLightfv(GL_LIGHT0, GL_SPECULAR, Color_On);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, Color_On); // global ambience
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // set these material colors
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, Color_Off);
    glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, Shininess);
#if MAC_GL_OPTIMIZE
    glDisable(GL_COLOR_MATERIAL);
#else
    glEnable(GL_COLOR_MATERIAL);
#endif

    // turn on culling to speed rendering
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // misc options
    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_NORMALIZE);
    glDisable(GL_SCISSOR_TEST);

    RecreateQuadric();
}


// methods dealing with the view

void OpenGLRenderer::NewView(double eyeTranslateToAngleDegrees) const
{
    if (cameraAngleRad <= 0.0) return;

//    TRACEMSG("Camera_distance: " << cameraDistance);
//    TRACEMSG("Camera_angle_rad: " << cameraAngleRad);
//    TRACEMSG("Camera_look_at_X: " << cameraLookAtX);
//    TRACEMSG("Camera_look_at_Y: " << cameraLookAtY);
//    TRACEMSG("Camera_clip_near: " << cameraClipNear);
//    TRACEMSG("Camera_clip_far: " << cameraClipFar);
//    TRACEMSG("projection: " << projectionType);

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

    // set camera angle/perspective
    if (projectionType == "Perspective") {
        gluPerspective(RadToDegrees(cameraAngleRad),    // viewing angle (degrees)
                       aspect,                          // w/h aspect
                       cameraClipNear,                  // near clipping plane
                       cameraClipFar);                  // far clipping plane
    } else { // Orthographic
        GLdouble right, top;
        top = ((cameraClipNear + cameraClipFar) / 2.0) * sin(cameraAngleRad / 2.0);
        right = top * aspect;
        glOrtho(-right,             // sides of viewing box, assuming eye is at (0,0,0)
                right,
                -top,
                top,
                cameraClipNear,     // near clipping plane
                cameraClipFar);     // far clipping plane
    }

    Vector cameraLoc(0.0, 0.0, cameraDistance);

    if (stereoOn && eyeTranslateToAngleDegrees != 0.0) {
        Vector view(cameraLookAtX, cameraLookAtY, -cameraDistance);
        Vector translate = vector_cross(view, Vector(0.0, 1.0, 0.0));
        translate.normalize();
        translate *= view.length() * tan(DegreesToRad(eyeTranslateToAngleDegrees));
        cameraLoc += translate;
    }
//    TRACEMSG("Camera X: " << cameraLoc.x);
//    TRACEMSG("Camera Y: " << cameraLoc.y);
//    TRACEMSG("Camera Z: " << cameraLoc.z);

    // set camera position and direction
    gluLookAt(cameraLoc.x, cameraLoc.y, cameraLoc.z,    // the camera position
              cameraLookAtX, cameraLookAtY, 0.0,        // the "look-at" point
              0.0, 1.0, 0.0);                           // the up direction

    glMatrixMode(GL_MODELVIEW);
}

void OpenGLRenderer::Display(void)
{
//    for (unsigned int m=0; m<16; ++m)
//        TRACEMSG("viewMatrix[" << m << "]: " << viewMatrix[m]);

    if (structureSet) {
        const Vector& background = structureSet->styleManager->GetBackgroundColor();
        glClearColor(background[0], background[1], background[2], 1.0);
    } else
        glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (selectMode) {
        glInitNames();
        glPushName(0);
    }

    GLint viewport[4] = {0, 0, 0, 0};
    double eyeSeparationDegrees = 0.0;
    if (stereoOn) {
        bool proximalStereo;
        glGetIntegerv(GL_VIEWPORT, viewport);
        if (!RegistryGetDouble(REG_ADVANCED_SECTION, REG_STEREO_SEPARATION, &eyeSeparationDegrees) ||
            !RegistryGetBoolean(REG_ADVANCED_SECTION, REG_PROXIMAL_STEREO, &proximalStereo))
        {
            ERRORMSG("OpenGLRenderer::Display() - error getting stereo settings from registry");
            return;
        }
        if (!proximalStereo)
            eyeSeparationDegrees = -eyeSeparationDegrees;
    }

    int first = 1, last = stereoOn ? 2 : 1;
    for (int e=first; e<=last; ++e) {

        glLoadMatrixd(viewMatrix);

        // adjust viewport & camera angle for stereo
        if (stereoOn) {
            if (e == first) {
                // left side
                glViewport(0, viewport[1], viewport[2] / 2, viewport[3]);
                NewView(eyeSeparationDegrees / 2);
            } else {
                // right side
                glViewport(viewport[2] / 2, viewport[1], viewport[2] - viewport[2] / 2, viewport[3]);
                NewView(-eyeSeparationDegrees / 2);
            }
        }

        if (structureSet) {
            if (currentFrame == ALL_FRAMES) {
                for (unsigned int i=FIRST_LIST; i<=structureSet->lastDisplayList; ++i) {
                    PushMatrix(*(structureSet->transformMap[i]));
                    glCallList(i);
                    PopMatrix();
                    AddTransparentSpheresForList(i);
                }
            } else {
                StructureSet::DisplayLists::const_iterator
                    l, le=structureSet->frameMap[currentFrame].end();
                for (l=structureSet->frameMap[currentFrame].begin(); l!=le; ++l) {
                    PushMatrix(*(structureSet->transformMap[*l]));
                    glCallList(*l);
                    PopMatrix();
                    AddTransparentSpheresForList(*l);
                }
            }

            // draw transparent spheres, which are already stored in view-transformed coordinates
            glLoadIdentity();
            RenderTransparentSpheres();
        }

        // draw logo if no structure
        else {
            glCallList(FIRST_LIST);
        }
    }

    glFinish();
    glFlush();

    // restore full viewport
    if (stereoOn)
        glViewport(0, viewport[1], viewport[2], viewport[3]);
}

void OpenGLRenderer::EnableStereo(bool enableStereo)
{
    TRACEMSG("turning " << (enableStereo ? "on" : "off" ) << " stereo");
    stereoOn = enableStereo;
    if (!stereoOn)
        NewView();
}

void OpenGLRenderer::ResetCamera(void)
{
    // set up initial camera
    glLoadIdentity();
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
        // allow a little "extra" room between clipping planes
        cameraClipNear = (cameraDistance - structureSet->maxDistFromCenter) * 0.66;
        cameraClipFar = (cameraDistance + structureSet->maxDistFromCenter) * 1.2;
        // move structureSet's center to origin
        glTranslated(-structureSet->center.x, -structureSet->center.y, -structureSet->center.z);
        structureSet->rotationCenter = structureSet->center;

    } else { // for logo
        cameraAngleRad = PI / 14;
        cameraDistance = 200;
        cameraClipNear = 0.5*cameraDistance;
        cameraClipFar = 1.5*cameraDistance;
    }

    // reset matrix
    glGetDoublev(GL_MODELVIEW_MATRIX, viewMatrix);
    NewView();
}

void OpenGLRenderer::ChangeView(eViewAdjust control, int dX, int dY, int X2, int Y2)
{
    bool doTranslation = false;
    Vector rotCenter;
    double pixelSize;
    GLint viewport[4];

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

#define MIN_CAMERA_ANGLE 0.001
#define MAX_CAMERA_ANGLE (0.999 * PI)

    switch (control) {
        case eXYRotateHV:
            glRotated(rotateSpeed*dY, 1.0, 0.0, 0.0);
            glRotated(rotateSpeed*dX, 0.0, 1.0, 0.0);
            break;

        case eZRotateH:
            glRotated(rotateSpeed*dX, 0.0, 0.0, 1.0);
            break;

        case eXYTranslateHV:
            glGetIntegerv(GL_VIEWPORT, viewport);
            pixelSize = tan(cameraAngleRad / 2.0) * 2.0 * cameraDistance / viewport[3];
            cameraLookAtX -= dX * pixelSize;
            cameraLookAtY += dY * pixelSize;
            NewView();
            break;

        case eZoomH:
            cameraAngleRad *= 1.0 - 0.01 * dX;
            if (cameraAngleRad < MIN_CAMERA_ANGLE) cameraAngleRad = MIN_CAMERA_ANGLE;
            else if (cameraAngleRad > MAX_CAMERA_ANGLE) cameraAngleRad = MAX_CAMERA_ANGLE;
            NewView();
            break;

        case eZoomHHVV:
            break;

        case eZoomOut:
            cameraAngleRad *= 1.5;
            if (cameraAngleRad > MAX_CAMERA_ANGLE) cameraAngleRad = MAX_CAMERA_ANGLE;
            NewView();
            break;

        case eZoomIn:
            cameraAngleRad /= 1.5;
            if (cameraAngleRad < MIN_CAMERA_ANGLE) cameraAngleRad = MIN_CAMERA_ANGLE;
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

void OpenGLRenderer::CenterView(const Vector& viewCenter, double radius)
{
    ResetCamera();

    structureSet->rotationCenter = viewCenter;

    Vector cameraLocation(0.0, 0.0, cameraDistance);
    Vector centerWRTcamera = viewCenter - structureSet->center;
    Vector direction = centerWRTcamera - cameraLocation;
    direction.normalize();
    double cosAngleZ = -direction.z;
    Vector lookAt = centerWRTcamera + direction * (centerWRTcamera.z / cosAngleZ);
    cameraLookAtX = lookAt.x;
    cameraLookAtY = lookAt.y;

    cameraAngleRad = 2.0 * atan(radius / (cameraLocation - centerWRTcamera).length());

    NewView();
    TRACEMSG("looking at " << lookAt << " angle " << RadToDegrees(cameraAngleRad));

    // do this so that this view is used upon restore
    SaveToASNViewSettings(NULL);
}

void OpenGLRenderer::ComputeBestView(void)
{
    if (!structureSet->IsMultiStructure() || !structureSet->CenterViewOnAlignedResidues())
        structureSet->CenterViewOnStructure();
}

void OpenGLRenderer::PushMatrix(const Matrix* m)
{
    glPushMatrix();
    if (m) {
        GLdouble g[16];
        Matrix2GL(*m, g);
        glMultMatrixd(g);
    }
}

void OpenGLRenderer::PopMatrix(void)
{
    glPopMatrix();
}

// display list management stuff
void OpenGLRenderer::StartDisplayList(unsigned int list)
{
    if (list >= FONT_BASE) {
        ERRORMSG("OpenGLRenderer::StartDisplayList() - too many display lists;\n"
            << "increase OpenGLRenderer::FONT_BASE");
        return;
    }
    ClearTransparentSpheresForList(list);
    SetColor(eResetCache); // reset color caches in SetColor

    glNewList(list, GL_COMPILE);
    currentDisplayList = list;

    if (currentDisplayList >= displayListEmpty.size()) displayListEmpty.resize(currentDisplayList + 1, true);
    displayListEmpty[currentDisplayList] = true;
}

void OpenGLRenderer::EndDisplayList(void)
{
    glEndList();
    currentDisplayList = NO_LIST;
}


// frame management methods

void OpenGLRenderer::ShowAllFrames(void)
{
    if (structureSet) currentFrame = ALL_FRAMES;
}

bool OpenGLRenderer::IsFrameEmpty(unsigned int frame) const
{
    if (!structureSet || structureSet->frameMap.size() <= frame) return false;

    StructureSet::DisplayLists::const_iterator l, le=structureSet->frameMap[frame].end();
    for (l=structureSet->frameMap[frame].begin(); l!=le; ++l)
        if (!displayListEmpty[*l])
            return false;
    return true;
}

void OpenGLRenderer::ShowFirstFrame(void)
{
    if (!structureSet || structureSet->frameMap.size() == 0) return;
    currentFrame = 0;
    while (IsFrameEmpty(currentFrame) && currentFrame < structureSet->frameMap.size() - 1)
        ++currentFrame;
}

void OpenGLRenderer::ShowLastFrame(void)
{
    if (!structureSet || structureSet->frameMap.size() == 0) return;
    currentFrame = structureSet->frameMap.size() - 1;
    while (IsFrameEmpty(currentFrame) && currentFrame > 0)
        --currentFrame;
}

void OpenGLRenderer::ShowNextFrame(void)
{
    if (!structureSet || structureSet->frameMap.size() == 0) return;
    if (currentFrame == ALL_FRAMES) currentFrame = structureSet->frameMap.size() - 1;
    unsigned int originalFrame = currentFrame;
    do {
        if (currentFrame == structureSet->frameMap.size() - 1)
            currentFrame = 0;
        else
            ++currentFrame;
    } while (IsFrameEmpty(currentFrame) && currentFrame != originalFrame);
}

void OpenGLRenderer::ShowPreviousFrame(void)
{
    if (!structureSet || structureSet->frameMap.size() == 0) return;
    if (currentFrame == ALL_FRAMES) currentFrame = 0;
    unsigned int originalFrame = currentFrame;
    do {
        if (currentFrame == 0)
            currentFrame = structureSet->frameMap.size() - 1;
        else
            --currentFrame;
    } while (IsFrameEmpty(currentFrame) && currentFrame != originalFrame);
}

void OpenGLRenderer::ShowFrameNumber(int frame)
{
    if (!structureSet) return;
    if (frame >= 0 && frame < (int)structureSet->frameMap.size() && !IsFrameEmpty(frame))
        currentFrame = frame;
    else
        currentFrame = ALL_FRAMES;
}

// process selection; return gl-name of result
bool OpenGLRenderer::GetSelected(int x, int y, unsigned int *name)
{
    // render with GL_SELECT mode, to fill selection buffer
    glSelectBuffer(pickBufSize, selectBuf);
    glRenderMode(GL_SELECT);
    selectMode = true;
    selectX = x;
    selectY = y;
    NewView();
    Display();
    GLint hits = glRenderMode(GL_RENDER);
    selectMode = false;
    NewView();

    // parse selection buffer to find name of selected item
    int i, j, p=0, n, top=0;
    GLuint minZ=0;
    *name = NO_NAME;
    for (i=0; i<hits; ++i) {
        n = selectBuf[p++];                 // # names
        if (i==0 || minZ > selectBuf[p]) {  // find item with min depth
            minZ = selectBuf[p];
            top = 1;
        } else
            top = 0;
        ++p;
        ++p;                                // skip max depth
        for (j=0; j<n; ++j) {               // loop through n names
            switch (j) {
                case 0:
                    if (top) *name = static_cast<unsigned int>(selectBuf[p]);
                    break;
                default:
                    WARNINGMSG("GL select: Got more than 1 name!");
            }
            ++p;
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
    currentFrame = ALL_FRAMES;
    if (!structureSet) initialViewFromASN.Reset();

    if (IsWindowedMode()) {
        Init();             // init GL system
        Construct();        // draw structures
        RestoreSavedView(); // load initial view if present
    }
}

void OpenGLRenderer::RecreateQuadric(void) const
{
#ifndef USE_MY_GLU_QUADS
    if (qobj)
        gluDeleteQuadric(qobj);
    if (!(qobj = gluNewQuadric())) {
        ERRORMSG("unable to create a new GLUQuadricObj");
        return;
    }
    gluQuadricDrawStyle(qobj, (GLenum) GLU_FILL);
    gluQuadricNormals(qobj, (GLenum) GLU_SMOOTH);
    gluQuadricOrientation(qobj, (GLenum) GLU_OUTSIDE);
    gluQuadricTexture(qobj, GL_FALSE);
#endif
}

void OpenGLRenderer::Construct(void)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (structureSet) {

        // get quality values from registry - assumes some values have been set already!
        if (!RegistryGetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, &atomSlices) ||
            !RegistryGetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, &atomStacks) ||
            !RegistryGetInteger(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, &bondSides) ||
            !RegistryGetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, &wormSides) ||
            !RegistryGetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, &wormSegments) ||
            !RegistryGetInteger(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, &helixSides) ||
            !RegistryGetBoolean(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, &highlightsOn) ||
            !RegistryGetString(REG_QUALITY_SECTION, REG_PROJECTION_TYPE, &projectionType))
            ERRORMSG("OpenGLRenderer::Construct() - error getting quality setting from registry");

        // do the drawing
        structureSet->DrawAll();

    } else {
        SetColor(eResetCache);
        ConstructLogo();
    }

    GlobalMessenger()->UnPostStructureRedraws();
}

void OpenGLRenderer::SetColor(OpenGLRenderer::EColorAction action,
    GLenum type, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
    static GLdouble cr, cg, cb, ca;
    static GLenum cachedType = GL_NONE, lastType = GL_NONE;

    if (action == eResetCache) {
        cachedType = lastType = GL_NONE;
        return;
    }

    if (action == eSetColorIfDifferent && type == lastType && red == cr && green == cg && blue == cb && alpha == ca)
        return;

    if (action == eUseCachedValues) {
        if (cachedType == GL_NONE) {
            ERRORMSG("can't do SetColor(eUseCachedValues) w/o previously doing eSetCacheValues or eSetColorIfDifferent");
            return;
        }
    } else {   // eSetCacheValues, or eSetColorIfDifferent and is different
        cachedType = (GLenum) type;
        cr = red;
        cg = green;
        cb = blue;
        ca = alpha;
        if (action == eSetCacheValues)
            return;
    }

    if (cachedType != lastType) {
        if (cachedType == GL_DIFFUSE) {
#ifndef MAC_GL_OPTIMIZE
            glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);     // needs to go before glMaterial calls for some reason, at least on PC
#endif
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, Color_MostlyOff);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, highlightsOn ? Color_Specular : Color_Off);
        } else if (cachedType == GL_AMBIENT) {
#ifndef MAC_GL_OPTIMIZE
            glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT);
#endif
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, Color_Off);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, Color_Off);
        } else {
            ERRORMSG("don't know how to handle material type " << cachedType);
        }
        lastType = cachedType;
    }

    GLfloat rgba[4] = { (GLfloat) cr, (GLfloat) cg, (GLfloat) cb, (GLfloat) ca };
#if MAC_GL_OPTIMIZE
	glMaterialfv(GL_FRONT_AND_BACK, cachedType, rgba);
#endif
    glColor4fv(rgba);
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

    ClearTransparentSpheresForList(FIRST_LIST);
    glNewList(FIRST_LIST, GL_COMPILE);

    /* create logo */
    SetColor(eSetColorIfDifferent, GL_DIFFUSE, logoColor[0], logoColor[1], logoColor[2]);

    for (n = 0; n < 2; ++n) { /* helix strand */
        if (n == 0) {
            startRad = maxRad;
            midRad = minRad;
            phase = 0;
        } else {
            startRad = minRad;
            midRad = maxRad;
            phase = PI;
        }
        for (g = 0; g <= segments; ++g) { /* segment (bottom to top) */

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
            for (s = 0; s < LOGO_SIDES; ++s) {
                V[0] = CR[0];
                V[2] = CR[2];
                V[1] = 0;
                length = sqrt(V[0]*V[0] + V[1]*V[1] + V[2]*V[2]);
                for (i = 0; i < 3; ++i) V[i] /= length;
                H[0] = H[2] = 0;
                H[1] = 1;
                for (i = 0; i < 3; ++i) {
                    pRingNorm[3*s + i] = V[i] * cos(PI * 2 * s / LOGO_SIDES) +
                                         H[i] * sin(PI * 2 * s / LOGO_SIDES);
                    pRingPts[3*s + i] = CR[i] + pRingNorm[3*s + i] * currentRad;
                }
            }
            if (g > 0) {
                glBegin(GL_TRIANGLE_STRIP);
                for (s = 0; s < LOGO_SIDES; ++s) {
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
                for (s = 0; s < LOGO_SIDES; ++s)
                    glVertex3d(pRingPts[3*s], pRingPts[3*s + 1], pRingPts[3*s + 2]);
            } else if (g == segments) {
                for (s = LOGO_SIDES - 1; s >= 0; --s)
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

// stuff dealing with rendering of transparent spheres

void OpenGLRenderer::AddTransparentSphere(const Vector& color, unsigned int name,
    const Vector& site, double radius, double alpha)
{
    if (currentDisplayList == NO_LIST) {
        WARNINGMSG("OpenGLRenderer::AddTransparentSphere() - not called during display list creation");
        return;
    }

    SphereList& spheres = transparentSphereMap[currentDisplayList];
    spheres.resize(spheres.size() + 1);
    SphereInfo& info = spheres.back();
    info.site = site;
    info.name = name;
    info.color = color;
    info.radius = radius;
    info.alpha = alpha;
}

// add spheres associated with this display list; calculate distance from camera to each one
void OpenGLRenderer::AddTransparentSpheresForList(unsigned int list)
{
    SphereMap::const_iterator sL = transparentSphereMap.find(list);
    if (sL == transparentSphereMap.end()) return;

    const SphereList& sphereList = sL->second;

    Matrix view;
    GL2Matrix(viewMatrix, &view);

    transparentSpheresToRender.resize(transparentSpheresToRender.size() + sphereList.size());
    SpherePtrList::reverse_iterator sph = transparentSpheresToRender.rbegin();

    SphereList::const_iterator i, ie=sphereList.end();
    const Matrix *dependentTransform;
    for (i=sphereList.begin(); i!=ie; ++i, ++sph) {
        sph->siteGL = i->site;
        dependentTransform = *(structureSet->transformMap[list]);
        if (dependentTransform)
            ApplyTransformation(&(sph->siteGL), *dependentTransform);               // dependent->master transform
        ApplyTransformation(&(sph->siteGL), view);                              // current view transform
        sph->distanceFromCamera = (Vector(0,0,cameraDistance) - sph->siteGL).length() - i->radius;
        sph->ptr = &(*i);
    }
}

void OpenGLRenderer::RenderTransparentSpheres(void)
{
    if (transparentSpheresToRender.size() == 0) return;

    // sort spheres by distance from camera, via operator < defined for SpherePtr
    transparentSpheresToRender.sort();

    // turn on blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SetColor(eResetCache); // reset color caches in SetColor

    // render spheres in order (farthest first)
    SpherePtrList::reverse_iterator i, ie=transparentSpheresToRender.rend();
    for (i=transparentSpheresToRender.rbegin(); i!=ie; ++i) {
        glLoadName(static_cast<GLuint>(i->ptr->name));
        glPushMatrix();
        glTranslated(i->siteGL.x, i->siteGL.y, i->siteGL.z);

        // apply a random spin so they're not all facing the same direction
        srand(static_cast<unsigned int>(fabs(i->ptr->site.x * 1000)));
        glRotated(360.0*rand()/RAND_MAX,
            1.0*rand()/RAND_MAX - 0.5, 1.0*rand()/RAND_MAX - 0.5, 1.0*rand()/RAND_MAX - 0.5);

#if MAC_GL_OPTIMIZE
        SetColor(eSetCacheValues, GL_DIFFUSE, i->ptr->color[0], i->ptr->color[1], i->ptr->color[2], i->ptr->alpha);
#else
        SetColor(eSetColorIfDifferent, GL_DIFFUSE, i->ptr->color[0], i->ptr->color[1], i->ptr->color[2], i->ptr->alpha);
#endif
        GLU_SPHERE(qobj, i->ptr->radius, atomSlices, atomStacks);
        glPopMatrix();
    }

    // turn off blending
    glDisable(GL_BLEND);

    // clear list; recreate it upon each Display()
    transparentSpheresToRender.clear();
}

void OpenGLRenderer::DrawAtom(const Vector& site, const AtomStyle& atomStyle)
{
    if (atomStyle.style == StyleManager::eNotDisplayed || atomStyle.radius <= 0.0)
        return;

    if (atomStyle.style == StyleManager::eSolidAtom) {
#if MAC_GL_OPTIMIZE
        SetColor(eSetCacheValues, GL_DIFFUSE, atomStyle.color[0], atomStyle.color[1], atomStyle.color[2]);
#else
        SetColor(eSetColorIfDifferent, GL_DIFFUSE, atomStyle.color[0], atomStyle.color[1], atomStyle.color[2]);
#endif
        glLoadName(static_cast<GLuint>(atomStyle.name));
        glPushMatrix();
        glTranslated(site.x, site.y, site.z);
        GLU_SPHERE(qobj, atomStyle.radius, atomSlices, atomStacks);
        glPopMatrix();
    }
    // need to delay rendering of transparent spheres
    else {
        AddTransparentSphere(atomStyle.color, atomStyle.name, site, atomStyle.radius, atomStyle.alpha);
        // but can put labels on now
        if (atomStyle.centerLabel.size() > 0)
            DrawLabel(atomStyle.centerLabel, site,
                (atomStyle.isHighlighted ? GlobalColors()->Get(Colors::eHighlight) : Vector(1,1,1)));
    }

    displayListEmpty[currentDisplayList] = false;
}

/* add a thick splined curve from point 1 *halfway* to point 2 */
void OpenGLRenderer::DrawHalfWorm(const Vector *p0, const Vector& p1,
    const Vector& p2, const Vector *p3,
    double radius, bool cap1, bool cap2,
    double tension)
{
    int i, j, k, m, offset=0;
    Vector R1, R2, Qt, p, dQt, H, V;
    double len, MG[4][3], T[4], t, prevlen=0.0, cosj, sinj;
    GLdouble *Nx=NULL, *Ny=NULL, *Nz=NULL, *Cx=NULL, *Cy=NULL, *Cz=NULL,
        *pNx=NULL, *pNy=NULL, *pNz=NULL, *pCx=NULL, *pCy=NULL, *pCz=NULL, *tmp;

    if (!p0 || !p3) {
        ERRORMSG("DrawHalfWorm: got NULL 0th and/or 3rd coords for worm");
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

    if (wormSides % 2) {
        WARNINGMSG("worm sides must be an even number");
        ++wormSides;
    }
    GLdouble *fblock = NULL;

    /* First, calculate the coordinate points of the center of the worm,
     * using the Kochanek-Bartels variant of the Hermite curve.
     */
    R1 = 0.5 * (1 - a) * (1 + b) * (1 + c) * (p1 - *p0) + 0.5 * (1 - a) * (1 - b) * (1 - c) * ( p2 - p1);
    R2 = 0.5 * (1 - a) * (1 + b) * (1 - c) * (p2 -  p1) + 0.5 * (1 - a) * (1 - b) * (1 + c) * (*p3 - p2);

    /*
     * Multiply MG=Mh.Gh, where Gh = [ P(1) P(2) R(1) R(2) ]. This
     * 4x1 matrix of vectors is constant for each segment.
     */
    for (i = 0; i < 4; ++i) {   /* calculate Mh.Gh */
        MG[i][0] = Mh[i][0] * p1.x + Mh[i][1] * p2.x + Mh[i][2] * R1.x + Mh[i][3] * R2.x;
        MG[i][1] = Mh[i][0] * p1.y + Mh[i][1] * p2.y + Mh[i][2] * R1.y + Mh[i][3] * R2.y;
        MG[i][2] = Mh[i][0] * p1.z + Mh[i][1] * p2.z + Mh[i][2] * R1.z + Mh[i][3] * R2.z;
    }

    for (i = 0; i <= wormSegments; ++i) {

        /* t goes from [0,1] from P(1) to P(2) (and we want to go halfway only),
           and the function Q(t) defines the curve of this segment. */
        t = (0.5 / wormSegments) * i;
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
                MAC_GL_SETCOLOR
                glVertex3d(p.x, p.y, p.z);
                MAC_GL_SETCOLOR
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
                fblock = new GLdouble[12 * wormSides];
                Nx = fblock;
                Ny = &Nx[wormSides];
                Nz = &Nx[wormSides * 2];
                Cx = &Nx[wormSides * 3];
                Cy = &Nx[wormSides * 4];
                Cz = &Nx[wormSides * 5];
                pNx = &Nx[wormSides * 6];
                pNy = &Nx[wormSides * 7];
                pNz = &Nx[wormSides * 8];
                pCx = &Nx[wormSides * 9];
                pCy = &Nx[wormSides * 10];
                pCz = &Nx[wormSides * 11];
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
            H.Set(0.0, -dQt.z, dQt.y);
            if (H.length() < 0.000001) /* nearly colinear - use [1,0.1,0] instead */
                H.Set(0.1 * dQt.z, -dQt.z, dQt.y - 0.1 * dQt.x);
            H.normalize();

            /* and a vertical vector = normal x H */
            V = vector_cross(dQt, H);
            V.normalize();

            /* finally, the worm circumference points (C) and normals (N) are
               simple trigonometric combinations of H and V */
            for (j = 0; j < wormSides; ++j) {
                cosj = cos(2 * PI * j / wormSides);
                sinj = sin(2 * PI * j / wormSides);
                Nx[j] = H.x * cosj + V.x * sinj;
                Ny[j] = H.y * cosj + V.y * sinj;
                Nz[j] = H.z * cosj + V.z * sinj;
                Cx[j] = Qt.x + Nx[j] * radius;
                Cy[j] = Qt.y + Ny[j] * radius;
                Cz[j] = Qt.z + Nz[j] * radius;
            }

            /* figure out which points on the previous circle "match" best
               with these, to minimize envelope twisting */
            if (i > 0) {
                for (m = 0; m < wormSides; ++m) {
                    len = 0.0;
                    for (j = 0; j < wormSides; ++j) {
                        k = j + m;
                        if (k >= wormSides)
                            k -= wormSides;
                        len += (Cx[k] - pCx[j]) * (Cx[k] - pCx[j]) +
                               (Cy[k] - pCy[j]) * (Cy[k] - pCy[j]) +
                               (Cz[k] - pCz[j]) * (Cz[k] - pCz[j]);
                    }
                    if (m == 0 || len < prevlen) {
                        prevlen = len;
                        offset = m;
                    }
                }
            }

            /* create triangles from points along this and previous circle */
            if (i > 0) {
                glBegin(GL_TRIANGLE_STRIP);
                for (j = 0; j < wormSides; ++j) {
                    k = j + offset;
                    if (k >= wormSides) k -= wormSides;
                    MAC_GL_SETCOLOR
                    glNormal3d(Nx[k], Ny[k], Nz[k]);
                    glVertex3d(Cx[k], Cy[k], Cz[k]);
                    MAC_GL_SETCOLOR
                    glNormal3d(pNx[j], pNy[j], pNz[j]);
                    glVertex3d(pCx[j], pCy[j], pCz[j]);
                }
                MAC_GL_SETCOLOR
                glNormal3d(Nx[offset], Ny[offset], Nz[offset]);
                glVertex3d(Cx[offset], Cy[offset], Cz[offset]);
                MAC_GL_SETCOLOR
                glNormal3d(pNx[0], pNy[0], pNz[0]);
                glVertex3d(pCx[0], pCy[0], pCz[0]);
                glEnd();
            }

            /* put caps on the end */
            if (cap1 && i == 0) {
                dQt.normalize();
                glBegin(GL_POLYGON);
                glNormal3d(-dQt.x, -dQt.y, -dQt.z);
                for (j = wormSides - 1; j >= 0; --j) {
                    MAC_GL_SETCOLOR
                    glVertex3d(Cx[j], Cy[j], Cz[j]);
                }
                glEnd();
            }
            else if (cap2 && i == wormSegments) {
                dQt.normalize();
                glBegin(GL_POLYGON);
                glNormal3d(dQt.x, dQt.y, dQt.z);
                for (j = 0; j < wormSides; ++j) {
                    k = j + offset;
                    if (k >= wormSides) k -= wormSides;
                    MAC_GL_SETCOLOR
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

    if (fblock) delete[] fblock;
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

void OpenGLRenderer::DrawHalfBond(const Vector& site1, const Vector& midpoint,
    StyleManager::eDisplayStyle style, double radius,
    bool cap1, bool cap2)
{
    // straight line bond
    if (style == StyleManager::eLineBond || (style == StyleManager::eCylinderBond && radius <= 0.0)) {
        glBegin(GL_LINES);
        MAC_GL_SETCOLOR
        glVertex3d(site1.x, site1.y, site1.z);
        MAC_GL_SETCOLOR
        glVertex3d(midpoint.x, midpoint.y, midpoint.z);
        glEnd();
    }

    // cylinder bond
    else if (style == StyleManager::eCylinderBond) {
        double length = (site1 - midpoint).length();
        if (length <= 0.000001 || bondSides <= 1) return;
        glPushMatrix();
        DoCylinderPlacementTransform(site1, midpoint, length);
        GLU_CYLINDER(qobj, radius, radius, length, bondSides, 1);
        if (cap1) {
            glPushMatrix();
            glRotated(180.0, 0.0, 1.0, 0.0);
            GLU_DISK(qobj, 0.0, radius, bondSides, 1);
            glPopMatrix();
        }
        if (cap2) {
            glPushMatrix();
            glTranslated(0.0, 0.0, length);
            GLU_DISK(qobj, 0.0, radius, bondSides, 1);
            glPopMatrix();
        }
        glPopMatrix();
    }
}

void OpenGLRenderer::DrawBond(const Vector& site1, const Vector& site2,
    const BondStyle& style, const Vector *site0, const Vector* site3)
{
    GLenum colorType;
    Vector midpoint = (site1 + site2) / 2;

    if (style.end1.style != StyleManager::eNotDisplayed) {
        colorType =
            (style.end1.style == StyleManager::eLineBond ||
            style.end1.style == StyleManager::eLineWormBond ||
            style.end1.radius <= 0.0)
                ? GL_AMBIENT : GL_DIFFUSE;
#if MAC_GL_OPTIMIZE
        SetColor(eSetCacheValues, colorType, style.end1.color[0], style.end1.color[1], style.end1.color[2]);
#else
        SetColor(eSetColorIfDifferent, colorType, style.end1.color[0], style.end1.color[1], style.end1.color[2]);
#endif
        glLoadName(static_cast<GLuint>(style.end1.name));
        if (style.end1.style == StyleManager::eLineWormBond ||
            style.end1.style == StyleManager::eThickWormBond)
            DrawHalfWorm(site0, site1, site2, site3,
                (style.end1.style == StyleManager::eThickWormBond) ? style.end1.radius : 0.0,
                style.end1.atomCap, style.midCap,
                style.tension);
        else
            DrawHalfBond(site1, midpoint,
                style.end1.style, style.end1.radius,
                style.end1.atomCap, style.midCap);
        displayListEmpty[currentDisplayList] = false;
    }

    if (style.end2.style != StyleManager::eNotDisplayed) {
        colorType =
            (style.end2.style == StyleManager::eLineBond ||
            style.end2.style == StyleManager::eLineWormBond ||
            style.end2.radius <= 0.0)
                ? GL_AMBIENT : GL_DIFFUSE;
#if MAC_GL_OPTIMIZE
        SetColor(eSetCacheValues, colorType, style.end2.color[0], style.end2.color[1], style.end2.color[2]);
#else
        SetColor(eSetColorIfDifferent, colorType, style.end2.color[0], style.end2.color[1], style.end2.color[2]);
#endif
        glLoadName(static_cast<GLuint>(style.end2.name));
        if (style.end2.style == StyleManager::eLineWormBond ||
            style.end2.style == StyleManager::eThickWormBond)
            DrawHalfWorm(site3, site2, site1, site0,
                (style.end2.style == StyleManager::eThickWormBond) ? style.end2.radius : 0.0,
                style.end2.atomCap, style.midCap,
                style.tension);
        else
            DrawHalfBond(midpoint, site2,
                style.end2.style, style.end2.radius,
                style.midCap, style.end2.atomCap);
        displayListEmpty[currentDisplayList] = false;
    }
}

void OpenGLRenderer::DrawHelix(const Vector& Nterm, const Vector& Cterm, const HelixStyle& style)
{
#if MAC_GL_OPTIMIZE
    SetColor(eSetCacheValues, GL_DIFFUSE, style.color[0], style.color[1], style.color[2]);
#else
    SetColor(eSetColorIfDifferent, GL_DIFFUSE, style.color[0], style.color[1], style.color[2]);
#endif
    glLoadName(static_cast<GLuint>(NO_NAME));

    double wholeLength = (Nterm - Cterm).length();
    if (wholeLength <= 0.000001) return;

    // transformation for whole helix
    glPushMatrix();
    DoCylinderPlacementTransform(Nterm, Cterm, wholeLength);

    // helix body
    double shaftLength =
        (style.style == StyleManager::eObjectWithArrow && style.arrowLength < wholeLength) ?
            wholeLength - style.arrowLength : wholeLength;
    GLU_CYLINDER(qobj, style.radius, style.radius, shaftLength, helixSides, 1);

    // Nterm cap
    glPushMatrix();
    glRotated(180.0, 0.0, 1.0, 0.0);
    GLU_DISK(qobj, 0.0, style.radius, helixSides, 1);
    glPopMatrix();

    // Cterm Arrow
    if (style.style == StyleManager::eObjectWithArrow && style.arrowLength < wholeLength) {
        // arrow base
        if (style.arrowBaseWidthProportion > 1.0) {
            glPushMatrix();
            glTranslated(0.0, 0.0, shaftLength);
            glRotated(180.0, 0.0, 1.0, 0.0);
            GLU_DISK(qobj, style.radius, style.radius * style.arrowBaseWidthProportion, helixSides, 1);
            glPopMatrix();
        }
        // arrow body
        glPushMatrix();
        glTranslated(0.0, 0.0, shaftLength);
        GLU_CYLINDER(qobj, style.radius * style.arrowBaseWidthProportion,
            style.radius * style.arrowTipWidthProportion, style.arrowLength, helixSides, 10);
        glPopMatrix();
        // arrow tip
        if (style.arrowTipWidthProportion > 0.0) {
            glPushMatrix();
            glTranslated(0.0, 0.0, wholeLength);
            GLU_DISK(qobj, 0.0, style.radius * style.arrowTipWidthProportion, helixSides, 1);
            glPopMatrix();
        }
    }

    // Cterm cap
    else {
        glPushMatrix();
        glTranslated(0.0, 0.0, wholeLength);
        GLU_DISK(qobj, 0.0, style.radius, helixSides, 1);
        glPopMatrix();
    }

    glPopMatrix();
    displayListEmpty[currentDisplayList] = false;
}

void OpenGLRenderer::DrawStrand(const Vector& Nterm, const Vector& Cterm,
    const Vector& unitNormal, const StrandStyle& style)
{
    GLdouble c000[3], c001[3], c010[3], c011[3],
             c100[3], c101[3], c110[3], c111[3], n[3];
    Vector a, h;
    int i;

#if MAC_GL_OPTIMIZE
    SetColor(eSetCacheValues, GL_DIFFUSE, style.color[0], style.color[1], style.color[2]);
#else
    SetColor(eSetColorIfDifferent, GL_DIFFUSE, style.color[0], style.color[1], style.color[2]);
#endif
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

    for (i=0; i<3; ++i) {
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

    for (i=0; i<3; ++i) n[i] = unitNormal[i];
    MAC_GL_SETCOLOR
    glNormal3dv(n);
    glVertex3dv(c010);
    MAC_GL_SETCOLOR
    glVertex3dv(c011);
    MAC_GL_SETCOLOR
    glVertex3dv(c111);
    MAC_GL_SETCOLOR
    glVertex3dv(c110);

    for (i=0; i<3; ++i) n[i] = -unitNormal[i];
    MAC_GL_SETCOLOR
    glNormal3dv(n);
    glVertex3dv(c000);
    MAC_GL_SETCOLOR
    glVertex3dv(c100);
    MAC_GL_SETCOLOR
    glVertex3dv(c101);
    MAC_GL_SETCOLOR
    glVertex3dv(c001);

    for (i=0; i<3; ++i) n[i] = h[i];
    MAC_GL_SETCOLOR
    glNormal3dv(n);
    glVertex3dv(c100);
    MAC_GL_SETCOLOR
    glVertex3dv(c110);
    MAC_GL_SETCOLOR
    glVertex3dv(c111);
    MAC_GL_SETCOLOR
    glVertex3dv(c101);

    for (i=0; i<3; ++i) n[i] = -h[i];
    MAC_GL_SETCOLOR
    glNormal3dv(n);
    glVertex3dv(c000);
    MAC_GL_SETCOLOR
    glVertex3dv(c001);
    MAC_GL_SETCOLOR
    glVertex3dv(c011);
    MAC_GL_SETCOLOR
    glVertex3dv(c010);

    for (i=0; i<3; ++i) n[i] = -a[i];
    MAC_GL_SETCOLOR
    glNormal3dv(n);
    glVertex3dv(c000);
    MAC_GL_SETCOLOR
    glVertex3dv(c010);
    MAC_GL_SETCOLOR
    glVertex3dv(c110);
    MAC_GL_SETCOLOR
    glVertex3dv(c100);

    if (style.style == StyleManager::eObjectWithoutArrow) {
        for (i=0; i<3; ++i) n[i] = a[i];
        MAC_GL_SETCOLOR
        glNormal3dv(n);
        glVertex3dv(c001);
        MAC_GL_SETCOLOR
        glVertex3dv(c101);
        MAC_GL_SETCOLOR
        glVertex3dv(c111);
        MAC_GL_SETCOLOR
        glVertex3dv(c011);

    } else {
        GLdouble FT[3], LT[3], RT[3], FB[3], LB[3], RB[3];

        for (i=0; i<3; ++i) {
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
        for (i=0; i<3; ++i) n[i] = -a[i];
        MAC_GL_SETCOLOR
        glNormal3dv(n);
        glVertex3dv(c111);
        MAC_GL_SETCOLOR
        glVertex3dv(LT);
        MAC_GL_SETCOLOR
        glVertex3dv(LB);
        MAC_GL_SETCOLOR
        glVertex3dv(c101);

        MAC_GL_SETCOLOR
        glVertex3dv(c011);
        MAC_GL_SETCOLOR
        glVertex3dv(c001);
        MAC_GL_SETCOLOR
        glVertex3dv(RB);
        MAC_GL_SETCOLOR
        glVertex3dv(RT);

        // the left side of the arrow
        for (i=0; i<3; ++i) h[i] = FT[i] - LT[i];
        Vector nL = vector_cross(unitNormal, h);
        nL.normalize();
        for (i=0; i<3; ++i) n[i] = nL[i];
        MAC_GL_SETCOLOR
        glNormal3dv(n);
        glVertex3dv(FT);
        MAC_GL_SETCOLOR
        glVertex3dv(FB);
        MAC_GL_SETCOLOR
        glVertex3dv(LB);
        MAC_GL_SETCOLOR
        glVertex3dv(LT);

        // the right side of the arrow
        for (i=0; i<3; ++i) h[i] = FT[i] - RT[i];
        Vector nR = vector_cross(h, unitNormal);
        nR.normalize();
        for (i=0; i<3; ++i) n[i] = nR[i];
        MAC_GL_SETCOLOR
        glNormal3dv(n);
        glVertex3dv(FT);
        MAC_GL_SETCOLOR
        glVertex3dv(RT);
        MAC_GL_SETCOLOR
        glVertex3dv(RB);
        MAC_GL_SETCOLOR
        glVertex3dv(FB);

        glEnd();

#ifdef __WXMAC__

		// the top and bottom arrow triangles; connect exactly to end points of base, otherwise
		// artifacts appear at junction on Mac (actually, they still do, but are much less obvious)
		glBegin(GL_TRIANGLE_FAN);
        for (i=0; i<3; ++i) n[i] = unitNormal[i];
        MAC_GL_SETCOLOR
        glNormal3dv(n);
        glVertex3dv(FT);
        MAC_GL_SETCOLOR
        glVertex3dv(LT);
        MAC_GL_SETCOLOR
        glVertex3dv(c011);
        MAC_GL_SETCOLOR
        glVertex3dv(c111);
        MAC_GL_SETCOLOR
        glVertex3dv(RT);
		glEnd();

		glBegin(GL_TRIANGLE_FAN);
        for (i=0; i<3; ++i) n[i] = -unitNormal[i];
        MAC_GL_SETCOLOR
        glNormal3dv(n);
        glVertex3dv(FB);
        MAC_GL_SETCOLOR
        glVertex3dv(RB);
        MAC_GL_SETCOLOR
        glVertex3dv(c101);
        MAC_GL_SETCOLOR
        glVertex3dv(c001);
        MAC_GL_SETCOLOR
        glVertex3dv(LB);

#else

        glBegin(GL_TRIANGLES);

        // the top and bottom arrow triangles
        for (i=0; i<3; ++i) n[i] = unitNormal[i];
        glNormal3dv(n);
        glVertex3dv(FT);
        glVertex3dv(LT);
        glVertex3dv(RT);

        for (i=0; i<3; ++i) n[i] = -unitNormal[i];
        glNormal3dv(n);
        glVertex3dv(FB);
        glVertex3dv(RB);
        glVertex3dv(LB);

#endif
    }

    glEnd();
    displayListEmpty[currentDisplayList] = false;
}

void OpenGLRenderer::DrawLabel(const string& text, const Vector& center, const Vector& color)
{
    int width, height, textCenterX = 0, textCenterY = 0;

    if (text.empty() || !glCanvas) return;
    
    if (!glCanvas->MeasureText(text, &width, &height, &textCenterX, &textCenterY))
        WARNINGMSG("MeasureText() failed, text may not be properly centered");

    SetColor(eSetColorIfDifferent, GL_AMBIENT, color[0], color[1], color[2]);
    glListBase(FONT_BASE);
    glRasterPos3d(center.x, center.y, center.z);
    glBitmap(0, 0, 0.0, 0.0, -textCenterX, -textCenterY, NULL);
    glCallLists(text.size(), GL_UNSIGNED_BYTE, text.data());
    glListBase(0);
    displayListEmpty[currentDisplayList] = false;
}

bool OpenGLRenderer::SaveToASNViewSettings(ncbi::objects::CCn3d_user_annotations *annotations)
{
    // save current camera settings
    initialViewFromASN.Reset(new CCn3d_view_settings());
    initialViewFromASN->SetCamera_distance(cameraDistance);
    initialViewFromASN->SetCamera_angle_rad(cameraAngleRad);
    initialViewFromASN->SetCamera_look_at_X(cameraLookAtX);
    initialViewFromASN->SetCamera_look_at_Y(cameraLookAtY);
    initialViewFromASN->SetCamera_clip_near(cameraClipNear);
    initialViewFromASN->SetCamera_clip_far(cameraClipFar);
    initialViewFromASN->SetMatrix().SetM0(viewMatrix[0]);
    initialViewFromASN->SetMatrix().SetM1(viewMatrix[1]);
    initialViewFromASN->SetMatrix().SetM2(viewMatrix[2]);
    initialViewFromASN->SetMatrix().SetM3(viewMatrix[3]);
    initialViewFromASN->SetMatrix().SetM4(viewMatrix[4]);
    initialViewFromASN->SetMatrix().SetM5(viewMatrix[5]);
    initialViewFromASN->SetMatrix().SetM6(viewMatrix[6]);
    initialViewFromASN->SetMatrix().SetM7(viewMatrix[7]);
    initialViewFromASN->SetMatrix().SetM8(viewMatrix[8]);
    initialViewFromASN->SetMatrix().SetM9(viewMatrix[9]);
    initialViewFromASN->SetMatrix().SetM10(viewMatrix[10]);
    initialViewFromASN->SetMatrix().SetM11(viewMatrix[11]);
    initialViewFromASN->SetMatrix().SetM12(viewMatrix[12]);
    initialViewFromASN->SetMatrix().SetM13(viewMatrix[13]);
    initialViewFromASN->SetMatrix().SetM14(viewMatrix[14]);
    initialViewFromASN->SetMatrix().SetM15(viewMatrix[15]);
    initialViewFromASN->SetRotation_center().SetX(structureSet->rotationCenter.x);
    initialViewFromASN->SetRotation_center().SetY(structureSet->rotationCenter.y);
    initialViewFromASN->SetRotation_center().SetZ(structureSet->rotationCenter.z);

    // store copy in given annotations
    if (annotations)
        annotations->SetView().Assign(*initialViewFromASN);

    return true;
}

bool OpenGLRenderer::LoadFromASNViewSettings(const ncbi::objects::CCn3d_user_annotations& annotations)
{
    initialViewFromASN.Reset();
    if (!annotations.IsSetView()) return true;
    TRACEMSG("Using view from incoming data...");

    // save a copy of the view settings
    initialViewFromASN.Reset(new CCn3d_view_settings);
    initialViewFromASN->Assign(annotations.GetView());
    return true;
}

void OpenGLRenderer::RestoreSavedView(void)
{
    if (initialViewFromASN.Empty() || !structureSet) {
        ResetCamera();
        return;
    }

    // restore current camera settings
    cameraDistance = initialViewFromASN->GetCamera_distance();
    cameraAngleRad = initialViewFromASN->GetCamera_angle_rad();
    cameraLookAtX = initialViewFromASN->GetCamera_look_at_X();
    cameraLookAtY = initialViewFromASN->GetCamera_look_at_Y();
    cameraClipNear = initialViewFromASN->GetCamera_clip_near();
    cameraClipFar = initialViewFromASN->GetCamera_clip_far();
    viewMatrix[0] = initialViewFromASN->GetMatrix().GetM0();
    viewMatrix[1] = initialViewFromASN->GetMatrix().GetM1();
    viewMatrix[2] = initialViewFromASN->GetMatrix().GetM2();
    viewMatrix[3] = initialViewFromASN->GetMatrix().GetM3();
    viewMatrix[4] = initialViewFromASN->GetMatrix().GetM4();
    viewMatrix[5] = initialViewFromASN->GetMatrix().GetM5();
    viewMatrix[6] = initialViewFromASN->GetMatrix().GetM6();
    viewMatrix[7] = initialViewFromASN->GetMatrix().GetM7();
    viewMatrix[8] = initialViewFromASN->GetMatrix().GetM8();
    viewMatrix[9] = initialViewFromASN->GetMatrix().GetM9();
    viewMatrix[10] = initialViewFromASN->GetMatrix().GetM10();
    viewMatrix[11] = initialViewFromASN->GetMatrix().GetM11();
    viewMatrix[12] = initialViewFromASN->GetMatrix().GetM12();
    viewMatrix[13] = initialViewFromASN->GetMatrix().GetM13();
    viewMatrix[14] = initialViewFromASN->GetMatrix().GetM14();
    viewMatrix[15] = initialViewFromASN->GetMatrix().GetM15();
    structureSet->rotationCenter.Set(
        initialViewFromASN->GetRotation_center().GetX(),
        initialViewFromASN->GetRotation_center().GetY(),
        initialViewFromASN->GetRotation_center().GetZ());

    NewView();
}

const wxFont& OpenGLRenderer::GetGLFont(void) const
{
    static const wxFont emptyFont;
    if (!glCanvas) {
        ERRORMSG("Can't call GetGLFont w/ NULL glCanvas");
        return emptyFont;
    }
    return glCanvas->GetGLFont();
}

bool OpenGLRenderer::SetGLFont(int firstChar, int nChars, int fontBase)
{
    bool okay = true;

#if defined(__WXMSW__)
    HDC hdc = wglGetCurrentDC();
    HGDIOBJ currentFont = SelectObject(hdc, reinterpret_cast<HGDIOBJ>(GetGLFont().GetHFONT()));
    if (!wglUseFontBitmaps(hdc, firstChar, nChars, fontBase)) {
        ERRORMSG("OpenGLRenderer::SetGLFont() - wglUseFontBitmaps() failed");
        okay = false;
    }
    SelectObject(hdc, currentFont);

#elif defined(__WXGTK__)
//  need to somehow get X font from wxFont... ugh.
//
//    PangoFont *pf = pango_font_map_load_font(
//        PangoFontMap *fontmap,
//        glCanvas->GtkGetPangoDefaultContext(),
//        GetGLFont().GetNativeFontInfo().description);
//
//    glXUseXFont(gdk_font_id(GetGLFont().GetInternalFont()), firstChar, nChars, fontBase);

#elif defined(__WXMAC__)

//  Offsets to font family, style determined empirically.  
//  'aglUseFont' deprecated w/o replacement as of OSX 10.5; code compiles but no longer 
//  appears to do anything useful.
    int fontFamily = GetGLFont().GetFamily() - wxFONTFAMILY_DEFAULT;
    int fontSize = GetGLFont().GetPointSize();
    int fontStyle = GetGLFont().GetStyle() - wxFONTSTYLE_NORMAL;
//    TRACEMSG("OpenGLRenderer::SetGLFont() - fontBase/fontFamily/fontSize/fontStyle " << fontBase << "/" << fontFamily << "/" << fontSize << "/" << fontStyle);

//  As aglUseFont no longer returns GL_TRUE, do not annoy users w/ the error message.
    if (aglUseFont(aglGetCurrentContext(),
                   (GLint) fontFamily, 
                   fontStyle, (GLint) fontSize,
                   firstChar, nChars, fontBase) != GL_TRUE) {
//        ERRORMSG("OpenGLRenderer::SetGLFont() - aglUseFont() failed: " << aglErrorString(aglGetError()));
        okay = false;
    }
#endif

    return okay;
}

END_SCOPE(Cn3D)
