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

#ifndef CN3D_OPENGL_RENDERER__HPP
#define CN3D_OPENGL_RENDERER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <list>
#include <map>
#include <vector>
#include <string>

#include <objects/cn3d/Cn3d_user_annotations.hpp>
#include <objects/cn3d/Cn3d_view_settings.hpp>

#include "vector_math.hpp"
#include "style_manager.hpp"

class wxFont;

// do not include GL headers here, so that other modules can more easily
// access this without potential name conflicts; use these instead by default,
// but will be set to proper types in opengl_renderer.cpp
#ifndef GL_ENUM_TYPE
#define GL_ENUM_TYPE int
#define GL_INT_TYPE int
#define GL_DOUBLE_TYPE double
#endif


BEGIN_SCOPE(Cn3D)

class StructureSet;
class AtomStyle;
class BondStyle;
class HelixStyle;
class StrandStyle;
class Cn3DGLCanvas;

class OpenGLRenderer
{
public:
    OpenGLRenderer(Cn3DGLCanvas *parentGLCanvas);

    // public data
    static const unsigned int NO_NAME;

    // public methods

    // calls once-only OpenGL initialization stuff (should be called after
    // the rendering context is established and the renderer made current)
    void Init(void) const;

    // tells the renderer that new camera settings need to be applied - should also
    // be called after window resize
    void NewView(double eyeTranslateToAngleDegrees = 0.0) const;

    // get the name
    bool GetSelected(int x, int y, unsigned int *name);

    // reset camera to full-view state
    void ResetCamera(void);

    // called to change view (according to mouse movements)
    enum eViewAdjust {
        eXYRotateHV,        // rotate about X,Y axes according to horiz. & vert. movement
        eZRotateH,          // rotate in plane (about Z) according to horiz. movement
        eXYTranslateHV,     // translate in X,Y according to horiz. & vert. movement
        eZoomH,             // zoom in/out with horiz. movement
        eZoomHHVV,          // zoom according to (H1,V1),(H2,V2) box
        eZoomIn,            // zoom in
        eZoomOut,           // zoom out
        eCenterCamera       // reset camera to look at origin
    };
    void ChangeView(eViewAdjust control, int dX = 0, int dY = 0, int X2 = 0, int Y2 = 0);

    // center the view on the given viewCenter point, and zoom the view according to radius
    void CenterView(const Vector& viewCenter, double radius);

    // draws the display lists
    void Display(void);

    // tells the renderer what structure(s) it's to draw
    void AttachStructureSet(StructureSet *targetStructureSet);
    void ComputeBestView(void);

    // constructs the structure display lists (but doesn't draw them)
    void Construct(void);

    // push the global view matrix, then apply transformation (e.g., for structure alignment)
    void PushMatrix(const Matrix* xform);
    // pop matrix
    void PopMatrix(void);

    // display list management
    static const unsigned int NO_LIST, FIRST_LIST, FONT_BASE;
    void StartDisplayList(unsigned int list);
    void EndDisplayList(void);

    // frame management
    void ShowAllFrames(void);
    void ShowFirstFrame(void);
    void ShowLastFrame(void);
    void ShowNextFrame(void);
    void ShowPreviousFrame(void);
    void ShowFrameNumber(int frame);

    // drawing methods
    void DrawAtom(const Vector& site, const AtomStyle& atomStyle);
    void DrawBond(const Vector& site1, const Vector& site2, const BondStyle& style,
        const Vector *site0, const Vector* site3);
    void DrawHelix(const Vector& Nterm, const Vector& Cterm, const HelixStyle& helixStyle);
    void DrawStrand(const Vector& Nterm, const Vector& Cterm,
        const Vector& unitNormal, const StrandStyle& strandStyle);
    void DrawLabel(const std::string& text, const Vector& center, const Vector& color);

    // load/save camera angle from/to asn data
    bool SaveToASNViewSettings(ncbi::objects::CCn3d_user_annotations *annotations);
    bool LoadFromASNViewSettings(const ncbi::objects::CCn3d_user_annotations& annotations);
    bool HasASNViewSettings(void) const { return !initialViewFromASN.Empty(); }

    // restore to saved view settings
    void RestoreSavedView(void);

    // set font used by OpenGL to the wxFont associated with the glCanvas
    bool SetGLFont(int firstChar, int nChars, int fontBase);
    const wxFont& GetGLFont(void) const;

    // stereo
    void EnableStereo(bool enableStereo);

    double GetRotateSpeed(void) const { return rotateSpeed; }

    void RecreateQuadric(void) const;

private:

    StructureSet *structureSet;
    Cn3DGLCanvas *glCanvas;

    enum EColorAction {
        eResetCache,                // reset cached values (no call to glMaterial)
        eSetCacheValues,            // set values in cache, but don't call glMaterial
        eUseCachedValues,           // set color with cached values
        eSetColorIfDifferent        // set color IFF values are different from cached values (+ set cache to new values)
    };
    void SetColor(EColorAction action, GL_ENUM_TYPE = 0,
        GL_DOUBLE_TYPE red = 0.0, GL_DOUBLE_TYPE green = 0.0, GL_DOUBLE_TYPE blue = 0.0, GL_DOUBLE_TYPE alpha = 1.0);

    // only defined #if USE_MY_GLU_QUADS (in opengl_renderer.cpp)
    void MyGluDisk(GL_DOUBLE_TYPE innerRadius, GL_DOUBLE_TYPE outerRadius, GL_INT_TYPE slices, GL_INT_TYPE loops);
    void MyGluCylinder(GL_DOUBLE_TYPE baseRadius, GL_DOUBLE_TYPE topRadius,
        GL_DOUBLE_TYPE height, GL_INT_TYPE slices, GL_INT_TYPE stacks);
    void MyGluSphere(GL_DOUBLE_TYPE radius, GL_INT_TYPE slices, GL_INT_TYPE stacks);

    void DrawHalfBond(const Vector& site1, const Vector& midpoint,
        StyleManager::eDisplayStyle style, double radius, bool cap1, bool cap2);
    void DrawHalfWorm(const Vector *p0, const Vector& p1,
        const Vector& p2, const Vector *p3, double radius, bool cap1, bool cap2, double tension);

    void ConstructLogo(void);

    // camera data
    double cameraDistance, cameraAngleRad,
        cameraLookAtX, cameraLookAtY,
        cameraClipNear, cameraClipFar,
        viewMatrix[16];
    ncbi::CRef < ncbi::objects::CCn3d_view_settings > initialViewFromASN;

    // controls for view changes
    double rotateSpeed;

    // misc rendering stuff
    bool selectMode;
    int selectX, selectY;
    unsigned int currentFrame;
    std::vector < bool > displayListEmpty;
    bool IsFrameEmpty(unsigned int frame) const;
    unsigned int currentDisplayList;

    // controls for stereo
    bool stereoOn;

    // stuff for rendering transparent spheres (done during Display())
public:
    typedef struct {
        Vector site, color;
        unsigned int name;
        double radius, alpha;
    } SphereInfo;
private:
    typedef std::list < SphereInfo > SphereList;
    typedef std::map < unsigned int, SphereList > SphereMap;
    SphereMap transparentSphereMap;
    void AddTransparentSphere(const Vector& color, unsigned int name,
        const Vector& site, double radius, double alpha);
    void ClearTransparentSpheresForList(unsigned int list)
    {
        SphereMap::iterator i = transparentSphereMap.find(list);
        if (i != transparentSphereMap.end())
            transparentSphereMap.erase(i);
    }

    class SpherePtr
    {
    public:
        Vector siteGL; // atom site in GL coordinates
        double distanceFromCamera;
        const SphereInfo *ptr;
        friend bool operator < (const SpherePtr& a, const SpherePtr& b)
            { return (a.distanceFromCamera < b.distanceFromCamera); }
    };
    typedef std::list < SpherePtr > SpherePtrList;
    SpherePtrList transparentSpheresToRender;
    void AddTransparentSpheresForList(unsigned int list);
    void RenderTransparentSpheres(void);
};

END_SCOPE(Cn3D)

#endif // CN3D_OPENGL_RENDERER__HPP
