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
* Revision 1.5  2000/08/17 14:24:07  thiessen
* added working StyleManager
*
* Revision 1.4  2000/08/13 02:43:02  thiessen
* added helix and strand objects
*
* Revision 1.3  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.2  2000/08/04 22:49:04  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.1  2000/08/03 15:13:59  thiessen
* add skeleton of style and show/hide managers
*
* ===========================================================================
*/

#include "cn3d/style_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/residue.hpp"
#include "cn3d/periodic_table.hpp"
#include "cn3d/bond.hpp"
#include "cn3d/show_hide_manager.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

void StyleSettings::SetToSecondaryStructure(void)
{
    proteinBackbone.type = nucleotideBackbone.type = eTrace;
    proteinBackbone.style = nucleotideBackbone.style = eTubeWorm;
    proteinBackbone.colorScheme = nucleotideBackbone.colorScheme = eObject;

    proteinSidechains.isOn = nucleotideSidechains.isOn = true;
    proteinSidechains.style = nucleotideSidechains.style = eWire;
    proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eElement;

    heterogens.isOn = true;
    heterogens.style = eBallAndStick;
    heterogens.colorScheme = eElement;

    solvents.isOn = true;
    solvents.style = eBallAndStick;
    solvents.colorScheme = eElement;

    connections.isOn = true;
    connections.style = eTubes;
    connections.colorScheme = eUserSelect;
    connections.userColor = Vector(1,1,0); // yellow
    
    helixObjects.isOn = strandObjects.isOn = true;
    helixObjects.style = strandObjects.style = eWithArrows;
    helixObjects.colorScheme = strandObjects.colorScheme = eSecondaryStructure;
    helixRadius = 1.8;
    strandWidth = 2.0;
    strandThickness = 0.5;

    proteinSidechains.userColor = nucleotideSidechains.userColor =
    proteinBackbone.userColor = nucleotideBackbone.userColor =
    heterogens.userColor = solvents.userColor =
    helixObjects.userColor = strandObjects.userColor = Vector(0.7,0.5,0.5);

    hydrogensOn = true;

    ballRadius = 0.4;
    stickRadius = 0.2;
    tubeRadius = 0.3;
    tubeWormRadius = 0.3;

    backgroundColor = Vector(0,0,0);
}

#define ATOM_NOT_DISPLAYED do { \
    atomStyle->style = eNotDisplayed; \
    return true; } while (0)

// get display style for atom, including show/hide status.
// May want to cache this eventually, since a
// particular atom's style may be queried several times per render (once for
// drawing atoms, and once for each bond to the atom).
bool StyleManager::GetAtomStyle(const Residue *residue, 
    const AtomPntr& atom, AtomStyle *atomStyle,
    const StyleSettings::BackboneStyle* *saveBackboneStyle,
    const StyleSettings::GeneralStyle* *saveGeneralStyle)
{
    if (!residue) {
        ERR_POST(Error << "StyleManager::GetAtomStyle() got NULL residue");
        return false;
    }
    const Molecule *molecule;
    if (!residue->GetParentOfType(&molecule)) return false;

    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return false;

    if (object->parentSet->showHideManager->IsResidueHidden(residue))
        ATOM_NOT_DISPLAYED;

    const StyleSettings& settings = GetStyleForResidue(object, atom.mID, atom.rID);
    const Residue::AtomInfo *info = residue->GetAtomInfo(atom.aID);
    if (info->atomicNumber == 1 && !settings.hydrogensOn)
        ATOM_NOT_DISPLAYED;
   
    // set up some pointers for more convenient access to style settings
    const StyleSettings::BackboneStyle *backboneStyle = NULL;
    const StyleSettings::GeneralStyle *generalStyle = NULL;
    if (info->classification == Residue::eAlphaBackboneAtom ||
        info->classification == Residue::ePartialBackboneAtom ||
        info->classification == Residue::eCompleteBackboneAtom) {
        if (residue->IsAminoAcid())
            backboneStyle = &(settings.proteinBackbone);
        else
            backboneStyle = &(settings.nucleotideBackbone);
    
    } else if (info->classification == Residue::eSideChainAtom) {
        if (residue->IsAminoAcid())
            generalStyle = &(settings.proteinSidechains);
        else
            generalStyle = &(settings.nucleotideSidechains);

    } else { // Residue::eUnknownAtom
        if (molecule->IsSolvent())
            generalStyle = &(settings.solvents);
        else if (molecule->IsHeterogen())
            generalStyle = &(settings.heterogens);
        else {
            ERR_POST(Error << "StyleManager::GetAtomStyle() - confused about style settings");
            return false;
        }
    }

    if ((!backboneStyle && !generalStyle) || (backboneStyle && generalStyle)) {
        ERR_POST(Error << "StyleManager::GetAtomStyle() - confused about style settings");
        return false;
    }
    if (saveBackboneStyle) *saveBackboneStyle = backboneStyle;
    if (saveGeneralStyle) *saveGeneralStyle = generalStyle;

    // first check whether this atom is visible, based on backbone and sidechain settings
    if (info->classification == Residue::eSideChainAtom && !generalStyle->isOn)
       ATOM_NOT_DISPLAYED;

    if (info->classification == Residue::eAlphaBackboneAtom ||
        info->classification == Residue::ePartialBackboneAtom ||
        info->classification == Residue::eCompleteBackboneAtom) { // is backbone of some sort

        // always allow alpha and C1* atoms for sidechain display
        if (info->classification != Residue::eAlphaBackboneAtom &&
            !(residue->IsNucleotide() && info->code == " C1*")) {

            // skip if backbone off
            if (backboneStyle->type == StyleSettings::eOff)
                ATOM_NOT_DISPLAYED;

            // show only alpha atom if eTrace
            if (backboneStyle->type == StyleSettings::eTrace)
                ATOM_NOT_DISPLAYED;

            // don't show complete backbone if set to partial
            if (info->classification == Residue::eCompleteBackboneAtom &&
                backboneStyle->type == StyleSettings::ePartial)
                ATOM_NOT_DISPLAYED;
        }
    }

    if (info->classification == Residue::eUnknownAtom && !generalStyle->isOn)
        ATOM_NOT_DISPLAYED;

    const Element *element = PeriodicTable.GetElement(info->atomicNumber);

    // determine radius
    StyleSettings::eDrawingStyle style;
    if (backboneStyle)
        style = backboneStyle->style;
    else
        style = generalStyle->style;

    switch (style) {
        case StyleSettings::eWire:
        case StyleSettings::eWireWorm:
        case StyleSettings::eTubeWorm:
            // don't do ATOM_NOT_DISPLAYED, because bonds still may need
            // style info about this atom
            atomStyle->radius = 0.0;
            break;
        case StyleSettings::eTubes:
            atomStyle->radius = settings.tubeRadius;
            break;
        case StyleSettings::eBallAndStick:
            atomStyle->radius = settings.ballRadius;
            break;
        case StyleSettings::eSpaceFill:
            atomStyle->radius = element->vdWRadius;
            break;
        default:
            ERR_POST(Error << "StyleManager::GetAtomStyle() - inappropriate style for atom");
            return false;
    }

    // make sure actual sphere isn't shown for C1* if not complete backbone
    if (info->residue->IsNucleotide() && info->code == " C1*" &&
        backboneStyle->type != StyleSettings::eComplete)
        atomStyle->radius = 0.0;

    // determine color
    StyleSettings::eColorScheme colorScheme;
    if (backboneStyle)
        colorScheme = backboneStyle->colorScheme;
    else
        colorScheme = generalStyle->colorScheme;

    switch (colorScheme) {
        case StyleSettings::eElement:
            atomStyle->color = element->color;
            break;
        case StyleSettings::eMolecule:
            // should actually be a color cycle...
        case StyleSettings::eObject:
            // should actually be a color cycle...
            if (object->IsMaster())
                atomStyle->color = Vector(1,0,1);
            else
                atomStyle->color = Vector(0,0,1);
            break;
        case StyleSettings::eSecondaryStructure:
            // needs to be done right, once residue->secondary structure lookup is in place
            atomStyle->color = Vector(0,1,1);
            break;
        case StyleSettings::eUserSelect:
            if (backboneStyle)
                atomStyle->color = backboneStyle->userColor;
            else
                atomStyle->color = generalStyle->userColor;
            break;
        default:
            ERR_POST(Error << "StyleManager::GetAtomStyle() - inappropriate color scheme for atom");
            return false;
    }

    atomStyle->style = eSphereAtom;
    atomStyle->name = info->glName;
    return true;
}

// this is basically a map from StyleSettings enums to StyleManager enums;
// sets bond radius, too
static bool SetBondStyleFromResidueStyle(StyleSettings::eDrawingStyle style,
    const StyleSettings& settings, BondStyle::EndStyle *end)
{
    switch (style) {
        case StyleSettings::eWire:
            end->style = StyleManager::eLineBond;
            break;
        case StyleSettings::eTubes:
            end->style = StyleManager::eCylinderBond;
            end->radius = settings.tubeRadius;
            break;
        case StyleSettings::eBallAndStick:
            end->style = StyleManager::eCylinderBond;
            end->radius = settings.stickRadius;
            break;
        case StyleSettings::eSpaceFill:
            end->style = StyleManager::eLineBond;
            break;
        case StyleSettings::eWireWorm:
            end->style = StyleManager::eLineWormBond;
            break;
        case StyleSettings::eTubeWorm:
            end->style = StyleManager::eThickWormBond;
            end->radius = settings.tubeWormRadius;
            break;
        default:
            ERR_POST(Error << "SetBondStyleFromResidueStyle() - invalid style for bond");
            return false;
    }
    return true;
}

#define BOND_NOT_DISPLAYED do { \
    bondStyle->end1.style = bondStyle->end2.style = StyleManager::eNotDisplayed; \
    return true; } while (0)

// Bond style is set by the residue style of the atoms at each end; the color
// is taken from the atom style (GetAtomStyle()), as well as some convenience
// style pointers (backboneStyle, generalStyle). Show/hide status is taken
// from the atoms - if either is hidden, the bond isn't shown either.
bool StyleManager::GetBondStyle(const Bond *bond,
        const AtomPntr& atom1, const AtomPntr& atom2,
        BondStyle *bondStyle)
{
    const StructureObject *object;
    if (!bond->GetParentOfType(&object)) return false;

    const Residue::AtomInfo
        *info1 = object->graph->GetAtomInfo(atom1), 
        *info2 = object->graph->GetAtomInfo(atom2);
    AtomStyle atomStyle1, atomStyle2;
    const StyleSettings::BackboneStyle *backboneStyle1, *backboneStyle2;
    const StyleSettings::GeneralStyle *generalStyle1, *generalStyle2;
    if (!GetAtomStyle(info1->residue, atom1, &atomStyle1, &backboneStyle1, &generalStyle1) ||
        !GetAtomStyle(info2->residue, atom2, &atomStyle2, &backboneStyle2, &generalStyle2))
        return false;

    // if either atom is hidden, don't display bond
    if (atomStyle1.style == eNotDisplayed ||
        atomStyle2.style == eNotDisplayed)
        BOND_NOT_DISPLAYED;

    // if this is a virtual bond, but style isn't eTrace, don't display
    if (bond->order == Bond::eVirtual &&
        (backboneStyle1->type != StyleSettings::eTrace ||
         backboneStyle2->type != StyleSettings::eTrace))
        BOND_NOT_DISPLAYED;

    bondStyle->end1.name = info1->glName;
    bondStyle->end2.name = info2->glName;
    bondStyle->end1.color = atomStyle1.color;
    bondStyle->end2.color = atomStyle2.color;
    bondStyle->end1.atomCap = bondStyle->end2.atomCap = bondStyle->midCap = false;

    // use connection style if bond is between molecules
    if (atom1.mID != atom2.mID) {
        bondStyle->end1.color = bondStyle->end2.color = globalStyle.connections.userColor;
        if (globalStyle.connections.style == StyleSettings::eWire)
            bondStyle->end1.style = bondStyle->end2.style = eLineBond;
        else if (globalStyle.connections.style == StyleSettings::eTubes) {
            bondStyle->end1.style = bondStyle->end2.style = eCylinderBond;
            bondStyle->end1.radius = bondStyle->end2.radius = globalStyle.tubeRadius;
            bondStyle->end1.atomCap = bondStyle->end2.atomCap = true;
        } else {
            ERR_POST(Error << "StyleManager::GetBondStyle() - invalid connection style");
            return false;
        }
        return true;
    }
    
    const StyleSettings&
        settings1 = GetStyleForResidue(object, atom1.mID, atom1.rID),
        settings2 = GetStyleForResidue(object, atom2.mID, atom2.rID);

    StyleSettings::eDrawingStyle style1;
    if (backboneStyle1)
        style1 = backboneStyle1->style;
    else
        style1 = generalStyle1->style;
    if (!SetBondStyleFromResidueStyle(style1, settings1, &(bondStyle->end1)))
        return false;

    StyleSettings::eDrawingStyle style2;
    if (backboneStyle2)
        style2 = backboneStyle2->style;
    else
        style2 = generalStyle2->style;
    if (!SetBondStyleFromResidueStyle(style2, settings2, &(bondStyle->end2)))
        return false;

    // don't show bonds between SpaceFill atoms
    if (style1 == StyleSettings::eSpaceFill && style2 == StyleSettings::eSpaceFill)
        BOND_NOT_DISPLAYED;

    // special case for bonds between side chain and residue - make whole bond
    // same style/color as side chain side
    if (info2->classification == Residue::eSideChainAtom &&
        (info1->classification == Residue::eAlphaBackboneAtom ||
         info1->classification == Residue::ePartialBackboneAtom ||
         info1->classification == Residue::eCompleteBackboneAtom)
        ) {
        bondStyle->end1.style = bondStyle->end2.style;
        bondStyle->end1.color = bondStyle->end2.color;
        bondStyle->end1.radius = bondStyle->end2.radius;
    } else if (info1->classification == Residue::eSideChainAtom &&
               (info2->classification == Residue::eAlphaBackboneAtom ||
                info2->classification == Residue::ePartialBackboneAtom ||
                info2->classification == Residue::eCompleteBackboneAtom)) {
        bondStyle->end2.style = bondStyle->end1.style;
        bondStyle->end2.color = bondStyle->end1.color;
        bondStyle->end2.radius = bondStyle->end1.radius;
    }

    // check options consistency, to avoid worm bonds between non-alpha atoms
    if ((info1->classification != Residue::eAlphaBackboneAtom &&
         (bondStyle->end1.style == eLineWormBond ||
          bondStyle->end1.style == eThickWormBond)) ||
        (info2->classification != Residue::eAlphaBackboneAtom &&
         (bondStyle->end2.style == eLineWormBond ||
          bondStyle->end2.style == eThickWormBond))) {
        ERR_POST(Error << "StyleManager::GetBondStyle() - can't have worm bonds between non-alpha atoms");
        return false;
    }
        
    // add midCap if style or radius for two sides of bond is different;
    if (bondStyle->end1.style != bondStyle->end2.style ||
        bondStyle->end1.radius != bondStyle->end2.radius)
        bondStyle->midCap = true;

    // atomCaps needed only at ends of thick worms
    if (bondStyle->end1.style == StyleManager::eThickWormBond &&
        !bond->previousVirtual)
        bondStyle->end1.atomCap = true; // add cap if no nextdoor virtual bond
    if (bondStyle->end2.style == StyleManager::eThickWormBond &&
        !bond->nextVirtual)
        bondStyle->end2.atomCap = true;

    // set worm tension, tighter for smaller protein alpha-helix
    if (bond->order == Bond::eVirtual) {
        if (info1->residue->IsAminoAcid())
            bondStyle->tension = -0.8;
        else
            bondStyle->tension = -0.4;
    }

    return true;
}

bool StyleManager::GetHelixStyle(const StructureObject *object,
    const Helix3D& helix, HelixStyle *helixStyle)
{
    if (!globalStyle.helixObjects.isOn) {
        helixStyle->style = StyleManager::eNotDisplayed;
        return true;
    }
    // should eventually check to see if the residues covered by the object are visible...

    // set drawing style
    if (globalStyle.helixObjects.style == StyleSettings::eWithArrows) {
        helixStyle->style = StyleManager::eObjectWithArrow;
        helixStyle->arrowLength = 4.0;
        helixStyle->arrowBaseWidthProportion = 1.2;
        helixStyle->arrowTipWidthProportion = 0.4;
    } else if (globalStyle.helixObjects.style == StyleSettings::eWithoutArrows) {
        helixStyle->style = StyleManager::eObjectWithoutArrow;
    } else {
        ERR_POST(Warning << "StyleManager::GetHelixStyle() - invalid helix style");
        return false;
    }

    helixStyle->radius = globalStyle.helixRadius;

    // set color
    switch (globalStyle.helixObjects.colorScheme) {
        case StyleSettings::eMolecule:
            // should actually be a color cycle...
        case StyleSettings::eObject:
            // should actually be a color cycle...
            if (object->IsMaster())
                helixStyle->color = Vector(1,0,1);
            else
                helixStyle->color = Vector(0,0,1);
            break;
        case StyleSettings::eSecondaryStructure:
            helixStyle->color = Vector(0,1,0);
            break;
        default:
            ERR_POST(Error << "StyleManager::GetHelixStyle() - inappropriate color scheme for helix");
            return false;
    }

    return true;
}

bool StyleManager::GetStrandStyle(const StructureObject *object,
    const Strand3D& strand, StrandStyle *strandStyle)
{
    if (!globalStyle.strandObjects.isOn) {
        strandStyle->style = StyleManager::eNotDisplayed;
        return true;
    }
    // should eventually check to see if the residues covered by the object are visible...

    // set drawing style
    if (globalStyle.strandObjects.style == StyleSettings::eWithArrows) {
        strandStyle->style = StyleManager::eObjectWithArrow;
        strandStyle->arrowLength = 2.8;
        strandStyle->arrowBaseWidthProportion = 1.6;
    } else if (globalStyle.strandObjects.style == StyleSettings::eWithoutArrows) {
        strandStyle->style = StyleManager::eObjectWithoutArrow;
    } else {
        ERR_POST(Warning << "StyleManager::GetStrandStyle() - invalid strand style");
        return false;
    }

    strandStyle->width = globalStyle.strandWidth;
    strandStyle->thickness = globalStyle.strandThickness;

    // set color
    switch (globalStyle.strandObjects.colorScheme) {
        case StyleSettings::eMolecule:
            // should actually be a color cycle...
        case StyleSettings::eObject:
            // should actually be a color cycle...
            if (object->IsMaster())
                strandStyle->color = Vector(1,0,1);
            else
                strandStyle->color = Vector(0,0,1);
            break;
        case StyleSettings::eSecondaryStructure:
            strandStyle->color = Vector(.7,.7,0);
            break;
        default:
            ERR_POST(Error << "StyleManager::GetStrandStyle() - inappropriate color scheme for strand");
            return false;
    }

    return true;
}

END_SCOPE(Cn3D)

