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
* ===========================================================================
*/

#ifndef CN3D_STYLE_MANAGER__HPP
#define CN3D_STYLE_MANAGER__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/cn3d/Cn3d_style_dictionary.hpp>
#include <objects/cn3d/Cn3d_style_settings.hpp>
#include <objects/cn3d/Cn3d_user_annotations.hpp>

#include <string>
#include <list>
#include <map>
#include <vector>

#include "vector_math.hpp"

class wxWindow;


BEGIN_SCOPE(Cn3D)

// for hydrophobicity and charge coloring
extern const double UNKNOWN_HYDROPHOBICITY;
extern double GetHydrophobicity(char code);
extern int GetCharge(char code);


// StyleSettings is a complete set of instructions on how to draw a set of
// molecules. It is used for "global" settings as well as for individual
// annotations. It is meant to contain all settings that can be saved on
// a per-output file basis.

// values of enumerations must match those in cn3d.asn!

class StyleSettings
{
public:
    // for different types of backbone displays
    enum eBackboneType {
        eOff = 1,
        eTrace = 2,
        ePartial = 3,
        eComplete = 4
    };

    // available drawing styles (not all necessarily applicable to all objects)
    enum eDrawingStyle {
        // for atoms and bonds
        eWire = 1,
        eTubes = 2,
        eBallAndStick = 3,
        eSpaceFill = 4,
        eWireWorm = 5,
        eTubeWorm = 6,

        // for 3d-objects
        eWithArrows = 7,
        eWithoutArrows = 8
    };

    // available color schemes (not all necessarily applicable to all objects)
    enum eColorScheme {
        eElement = 1,
        eObject = 2,
        eMolecule = 3,
        eDomain = 4,
        eResidue = 20,
        eSecondaryStructure = 5,
        eTemperature = 13,
        eHydrophobicity = 14,
        eCharge = 15,
        eRainbow = 16,
        eUserSelect = 6,

        // different alignment conservation coloring (currently only used for proteins)
        eAligned = 7,
        eIdentity = 8,
        eVariety = 9,
        eWeightedVariety = 10,
        eInformationContent = 11,
        eFit = 12,
        eBlockFit = 17,
        eBlockZFit = 18,
        eBlockRowFit = 19
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

    enum eLabelType {
        eOneLetter = 1,
        eThreeLetter = 2
    };

    enum eNumberType {
        eNoNumbers = 0,
        eSequentialNumbering = 1,   // from 1 by residues present, to match sequence loc
        ePDBNumbering = 2           // use number assigned by PDB
    };

    typedef struct {
        int spacing;
        eLabelType type;
        eNumberType numbering;
        bool
            terminiOn,              // label chain termini
            white;                  // use white labels (or if bg is light, use black for contrast)
    } LabelStyle;

    BackboneStyle proteinBackbone, nucleotideBackbone;

    GeneralStyle
        proteinSidechains, nucleotideSidechains,
        heterogens,
        solvents,
        connections,
        helixObjects, strandObjects;

    LabelStyle proteinLabels, nucleotideLabels;

    bool virtualDisulfidesOn, hydrogensOn, ionLabelsOn;

    Vector virtualDisulfideColor, backgroundColor;

    double spaceFillProportion, ballRadius, stickRadius, tubeRadius, tubeWormRadius,
        helixRadius, strandWidth, strandThickness;

    // to set to predetermined renderings
    enum ePredefinedRenderingStyle {
        eWormShortcut,
        eTubeShortcut,
        eWireframeShortcut,
        eBallAndStickShortcut,
        eSpacefillShortcut,
        eToggleSidechainsShortcut
    };
    void SetRenderingStyle(ePredefinedRenderingStyle style);

    // to set to predetermined colors
    enum ePredefinedColorScheme {
        eSecondaryStructureShortcut,
        eAlignedShortcut,
        eIdentityShortcut,
        eVarietyShortcut,
        eWeightedVarietyShortcut,
        eInformationContentShortcut,
        eFitShortcut,
        eBlockFitShortcut,
        eBlockZFitShortcut,
        eBlockRowFitShortcut,
        eObjectShortcut,
        eDomainShortcut,
        eMoleculeShortcut,
        eResidueShortcut,
        eRainbowShortcut,
        eHydrophobicityShortcut,
        eChargeShortcut,
        eTemperatureShortcut,
        eElementShortcut
    };
    void SetColorScheme(ePredefinedColorScheme scheme);

    // default labeling style
    void SetDefaultLabeling(void);

    // default and copy constructors
    StyleSettings(void)
    {
        SetRenderingStyle(eWormShortcut);
        SetColorScheme(eSecondaryStructureShortcut);
        SetDefaultLabeling();
    }
    StyleSettings(const StyleSettings& orig) { *this = orig; }

    // copy settings
    StyleSettings& operator = (const StyleSettings& v);

    // to convert to/from asn
    bool SaveSettingsToASN(ncbi::objects::CCn3d_style_settings *styleASN) const;
    bool LoadSettingsFromASN(const ncbi::objects::CCn3d_style_settings& styleASN);
};


class StructureSet;
class StructureObject;
class Residue;
class AtomPntr;
class AtomCoord;
class Bond;
class BondStyle;
class AtomStyle;
class Object3D;
class ObjectStyle;
class Helix3D;
class HelixStyle;
class Strand3D;
class StrandStyle;
class Molecule;
class MoleculeIdentifier;
class AnnotateDialog;

class StyleManager
{
public:
    StyleManager(const StructureSet *set);

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
    bool GetAtomStyle(const Residue *residue, const AtomPntr& atom,
        const AtomCoord *coord, AtomStyle *atomStyle,
        const StyleSettings::BackboneStyle* *saveBackboneStyle = NULL,
        const StyleSettings::GeneralStyle* *saveGeneralStyle = NULL) const;
    bool GetBondStyle(const Bond *bond,
            const AtomPntr& atom1, const AtomCoord *coord1,
            const AtomPntr& atom2, const AtomCoord *coord2,
            double bondLength, BondStyle *bondStyle) const;
    bool GetHelixStyle(const StructureObject *object,
        const Helix3D& helix, HelixStyle *helixStyle) const;
    bool GetStrandStyle(const StructureObject *object,
        const Strand3D& strand, StrandStyle *strandStyle) const;

    // bring up dialog to edit global style; returns true if style changed
    bool EditGlobalStyle(wxWindow *parent);

    // edit user annotations; returns true if any style changed
    bool EditUserAnnotations(wxWindow *parent);

    // check style option consistency
    bool CheckStyleSettings(StyleSettings *settings);
    bool CheckGlobalStyleSettings(void);

    // load/save asn style dictionary to/from current styles
    ncbi::objects::CCn3d_style_dictionary * CreateASNStyleDictionary(void) const;
    bool LoadFromASNStyleDictionary(const ncbi::objects::CCn3d_style_dictionary& styleDictionary);

    // add a new user style with default settings; returns true if successful,
    // and sets style ID assigned to the new style and pointer to the new style structure
    bool AddUserStyle(int *id, StyleSettings **newStyle);

    // remove a style; returns false if a user style of the given ID is not found
    bool RemoveUserStyle(int id);

    // StyleSettings accessors
    const StyleSettings& GetGlobalStyle(void) const { return globalStyle; }
    const StyleSettings& GetStyleForResidue(const StructureObject *object,
        int moleculeID, int residueID) const;
    const Vector& GetObjectColor(const Molecule *molecule) const;
    const StyleSettings * GetUserStyle(int id) const
    {
        StyleMap::const_iterator style = userStyles.find(id);
        return ((style != userStyles.end()) ? &(style->second) : NULL);
    }
    StyleSettings * GetUserStyle(int id)
    {
        StyleMap::iterator style = userStyles.find(id);
        return ((style != userStyles.end()) ? &(style->second) : NULL);
    }

    // predefined styles
    void SetGlobalColorScheme(StyleSettings::ePredefinedColorScheme scheme);
    void SetGlobalRenderingStyle(StyleSettings::ePredefinedRenderingStyle style);

    // set global style
    bool SetGlobalStyle(const ncbi::objects::CCn3d_style_settings& styleASN);

    // load/save asn user annotations
    bool SaveToASNUserAnnotations(ncbi::objects::CCn3d_user_annotations *annotations) const;
    bool LoadFromASNUserAnnotations(const ncbi::objects::CCn3d_user_annotations& annotations);

    // typedefs for UserAnnotation
    typedef std::map < const MoleculeIdentifier * , std::vector < bool > > ResidueMap;
    class UserAnnotation : public ncbi::CObject {
    public:
        std::string name, description;
        int styleID;
        ResidueMap residues;
        bool isDisplayed;
    };
    typedef std::list < ncbi::CRef < UserAnnotation > > UserAnnotationList;

    // add a new (empty) annotation; returns a pointer to the new UserAnnotation structure
    UserAnnotation * AddUserAnnotation(void);

    // remove an annotation; returns false if the given annotation isn't found
    bool RemoveUserAnnotation(UserAnnotation *annotation);

    // sets the displayed status of the given annotation; returns false if annotation not found.
    // Should use this rather than setting UserAnnotation.isDisplayed directly.
    bool DisplayUserAnnotation(UserAnnotation *annotation, bool display);

    // moves the priority of annotation up (moveUp==true) or down (moveUp==false);
    // returns false if annotation not found
    bool MoveUserAnnotation(UserAnnotation *annotation, bool moveUp);

    // annotation list accessors
    UserAnnotationList& GetUserAnnotations(void) { return userAnnotations; }
    const UserAnnotationList& GetUserAnnotations(void) const { return userAnnotations; }

private:
    const StructureSet *structureSet;
    StyleSettings globalStyle;

    // a set of user styles, each with its own unique integer id
    typedef std::map < int , StyleSettings > StyleMap;
    StyleMap userStyles;

    // the priority-ordered list of annotations, including which are currently displayed;
    // lowest-indexed annotation has highest priority (to reflect GUI list where the
    // annotation on top of the stack has priority)
    UserAnnotationList userAnnotations;

    bool GetObjectStyle(const StructureObject *object, const Object3D& object3D,
        const StyleSettings::GeneralStyle& generalStyle, ObjectStyle *objectStyle) const;
};

// the following are convenience containers to tell the Draw functions how
// to render various individual objects

class AtomStyle
{
public:
    StyleManager::eDisplayStyle style;
    Vector color;
    double radius, alpha;
    unsigned int name;
    std::string centerLabel;
    bool isHighlighted;
};

class BondStyle
{
public:
    typedef struct {
        StyleManager::eDisplayStyle style;
        Vector color;
        double radius;
        bool atomCap;
        unsigned int name;
    } EndStyle;
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
