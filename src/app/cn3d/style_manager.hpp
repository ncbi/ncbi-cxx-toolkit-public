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
*      manager object to track drawing style of objects at various levels
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2000/09/20 22:22:03  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.14  2000/09/11 22:57:56  thiessen
* working highlighting
*
* Revision 1.13  2000/09/11 14:06:03  thiessen
* working alignment coloring
*
* Revision 1.12  2000/08/24 23:39:54  thiessen
* add 'atomic ion' labels
*
* Revision 1.11  2000/08/24 18:43:15  thiessen
* tweaks for transparent sphere display
*
* Revision 1.10  2000/08/21 19:31:17  thiessen
* add style consistency checking
*
* Revision 1.9  2000/08/21 17:22:45  thiessen
* add primitive highlighting for testing
*
* Revision 1.8  2000/08/18 18:57:44  thiessen
* added transparent spheres
*
* Revision 1.7  2000/08/17 18:32:38  thiessen
* minor fixes to StyleManager
*
* Revision 1.6  2000/08/17 14:22:01  thiessen
* added working StyleManager
*
* Revision 1.5  2000/08/13 02:42:13  thiessen
* added helix and strand objects
*
* Revision 1.4  2000/08/11 12:59:13  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.3  2000/08/07 00:20:19  thiessen
* add display list mechanism
*
* Revision 1.2  2000/08/04 22:49:11  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.1  2000/08/03 15:14:18  thiessen
* add skeleton of style and show/hide managers
*
* ===========================================================================
*/

#ifndef CN3D_STYLE_MANAGER__HPP
#define CN3D_STYLE_MANAGER__HPP

#include <string>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// StyleSettings is a complete set of instructions on how to draw a set of
// molecules. It is used for "global" settings as well as for individual
// annotations. It is meant to contain all settings that can be saved on
// a per-output file basis.

class StyleSettings
{
public:
    // for different types of backbone displays
    enum eBackboneType {
        eOff,
        eTrace,
        ePartial,
        eComplete
    };

    // available drawing styles (not all necessarily applicable to all objects)
    enum eDrawingStyle {
        // for atoms and bonds
        eWire,
        eTubes,
        eBallAndStick,
        eSpaceFill,
        eWireWorm,
        eTubeWorm,

        // for 3d-objects
        eWithArrows,
        eWithoutArrows
    };

    // available color schemes (not all necessarily applicable to all objects)
    enum eColorScheme {
        eElement,
        eObject,
        eMolecule,
        eSecondaryStructure,
        eUserSelect,

        // different alignment conservation coloring (currently only used for proteinBackbone)
        eAligned,
        eIdentity,
        eVariety,
        eWeightedVariety
    };

    typedef struct {
        eBackboneType type;
        eDrawingStyle style;
        eColorScheme colorScheme;
        Vector userColor;
    } BackboneStyle;
    
    typedef struct {
        bool isOn;
        eDrawingStyle style;
        eColorScheme colorScheme;
        Vector userColor;
    } GeneralStyle;
    
    BackboneStyle proteinBackbone, nucleotideBackbone;

    GeneralStyle 
        proteinSidechains, nucleotideSidechains,
        heterogens,
        solvents,
        connections,
        helixObjects, strandObjects;
    
    bool hydrogensOn;

    double ballRadius, stickRadius, tubeRadius, tubeWormRadius,
        helixRadius, strandWidth, strandThickness;
    Vector backgroundColor;

    // methods to set to predetermined states
    void SetToSecondaryStructure(void);
    void SetToWireframe(void);
    void SetToAlignment(StyleSettings::eColorScheme protBBType);

    StyleSettings::StyleSettings(void) { SetToSecondaryStructure(); }
};

class StructureSet;
class StructureObject;
class Residue;
class AtomPntr;
class Bond;
class BondStyle;
class AtomStyle;
class Object3D;
class ObjectStyle;
class Helix3D;
class HelixStyle;
class Strand3D;
class StrandStyle;

class StyleManager
{
public:
    StyleManager(void);

    // display styles for various types of objects
    enum eDisplayStyle {
        eSolidAtom,
        eTransparentAtom,
        eLineBond,
        eCylinderBond,
        eLineWormBond,
        eThickWormBond,
        eObjectWithArrow,
        eObjectWithoutArrow,
        eNotDisplayed
    };

    const Vector& GetBackgroundColor(void) const { return globalStyle.backgroundColor; }

    // style accessors for individual objects
    bool GetAtomStyle(const Residue *residue, const AtomPntr& atom, AtomStyle *atomStyle,
        const StyleSettings::BackboneStyle* *saveBackboneStyle = NULL,
        const StyleSettings::GeneralStyle* *saveGeneralStyle = NULL) const;
    bool GetBondStyle(const Bond *bond,
            const AtomPntr& atom1, const AtomPntr& atom2, double bondLength,
            BondStyle *bondStyle) const;
    bool GetHelixStyle(const StructureObject *object,
        const Helix3D& helix, HelixStyle *helixStyle) const;
    bool GetStrandStyle(const StructureObject *object,
        const Strand3D& strand, StrandStyle *strandStyle) const;

    // check style option consistency
    bool CheckStyleSettings(const StructureSet *set);

private:
    StyleSettings globalStyle;

    bool GetObjectStyle(const StructureObject *object, const Object3D& object3D,
        const StyleSettings::GeneralStyle& generalStyle, ObjectStyle *objectStyle) const;

public:
    // StyleSettings accessors
    const StyleSettings& GetGlobalStyle(void) const { return globalStyle; }
    const StyleSettings& GetStyleForResidue(const StructureObject *object,
        int moleculeID, int residueID) const;

    void SetToSecondaryStructure(void) { globalStyle.SetToSecondaryStructure(); }
    void SetToWireframe(void) { globalStyle.SetToWireframe(); }
    void SetToAlignment(StyleSettings::eColorScheme protBBType)
        { globalStyle.SetToAlignment(protBBType); }
};

// the following are convenience containers to tell the Draw functions how
// to render various individual objects

class AtomStyle
{
public:
    StyleManager::eDisplayStyle style;
    Vector color;
    double radius;
    unsigned int name;
    std::string centerLabel;
    bool isHighlighted;
};

class BondStyle
{
public:
    typedef struct EndStyle {
        StyleManager::eDisplayStyle style;
        Vector color;
        double radius;
        bool atomCap;
        unsigned int name;
        bool isHighlighted;
    };
    EndStyle end1, end2;
    bool midCap;
    double tension;
};

class ObjectStyle
{
public:
    StyleManager::eDisplayStyle style;
    Vector color;
    double arrowLength, arrowBaseWidthProportion;
};

class HelixStyle : public ObjectStyle
{
public:
    double radius, arrowTipWidthProportion;
};

class StrandStyle : public ObjectStyle
{
public:
    double width, thickness;
};

END_SCOPE(Cn3D)

#endif // CN3D_STYLE_MANAGER__HPP
