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

    // get current gl viewport - array of four int's: x, y, width, height
    void GetViewport(int *viewport);

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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.46  2004/08/13 18:26:45  thiessen
* continue with Mac GL optimization, as well as adding local GLU quadric equivalents (disk, cylinder, sphere)
*
* Revision 1.45  2004/08/12 17:28:55  thiessen
* begin mac gl optimization process
*
* Revision 1.44  2004/08/09 19:17:48  thiessen
* rewrite SetColor to use glColorMaterial+glColor, and prepare for caching
*
* Revision 1.43  2004/02/19 17:05:00  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.42  2003/12/04 15:58:37  thiessen
* fix const problem
*
* Revision 1.41  2003/12/04 15:49:41  thiessen
* fix stereo and PNG export problems on Mac
*
* Revision 1.40  2003/12/03 15:46:36  thiessen
* adjust so spin increment is accurate
*
* Revision 1.39  2003/11/15 16:08:36  thiessen
* add stereo
*
* Revision 1.38  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.37  2002/09/14 17:03:07  thiessen
* center initial view on aligned residues
*
* Revision 1.36  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.35  2001/10/23 13:53:38  thiessen
* add PNG export
*
* Revision 1.34  2001/10/17 17:46:27  thiessen
* save camera setup and rotation center in files
*
* Revision 1.33  2001/09/06 18:17:01  thiessen
* fix OpenGL window initialization/OnSize to work on Mac
*
* Revision 1.32  2001/08/24 00:40:57  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.31  2001/08/13 22:30:52  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.30  2001/08/09 19:07:19  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.29  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.28  2001/06/15 14:52:30  thiessen
* fix minor syntax errors
*
* Revision 1.27  2001/05/22 19:09:09  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.26  2001/05/17 18:34:00  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.25  2001/05/16 00:07:23  thiessen
* one more minor fix
*
* Revision 1.24  2001/05/15 23:49:20  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.23  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.22  2000/11/02 16:48:22  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.21  2000/10/04 17:40:46  thiessen
* rearrange STL #includes
*
* Revision 1.20  2000/08/30 19:49:03  thiessen
* working sequence window
*
* Revision 1.19  2000/08/25 14:21:32  thiessen
* minor tweaks
*
* Revision 1.18  2000/08/24 23:39:54  thiessen
* add 'atomic ion' labels
*
* Revision 1.17  2000/08/21 17:22:45  thiessen
* add primitive highlighting for testing
*
* Revision 1.16  2000/08/19 02:58:23  thiessen
* fix transparent sphere bug
*
* Revision 1.15  2000/08/18 23:07:03  thiessen
* minor efficiency tweaks
*
* Revision 1.14  2000/08/18 18:57:43  thiessen
* added transparent spheres
*
* Revision 1.13  2000/08/17 14:22:00  thiessen
* added working StyleManager
*
* Revision 1.12  2000/08/13 02:42:13  thiessen
* added helix and strand objects
*
* Revision 1.11  2000/08/11 12:59:13  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.10  2000/08/07 14:12:48  thiessen
* added animation frames
*
* Revision 1.9  2000/08/07 00:20:18  thiessen
* add display list mechanism
*
* Revision 1.8  2000/08/04 22:49:10  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.7  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.6  2000/07/18 00:05:45  thiessen
* allow arbitrary rotation center
*
* Revision 1.5  2000/07/17 22:36:46  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.4  2000/07/17 04:21:09  thiessen
* now does correct structure alignment transformation
*
* Revision 1.3  2000/07/16 23:18:34  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/12 23:28:27  thiessen
* now draws basic CPK model
*
* Revision 1.1  2000/07/12 14:10:45  thiessen
* added initial OpenGL rendering engine
*
*/
