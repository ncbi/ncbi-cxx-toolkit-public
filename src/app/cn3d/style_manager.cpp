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

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

bool StyleManager::GetAtomStyle(const StructureObject *object, 
    const AtomPntr& atom, AtomStyle *atomStyle)
{
    if (!object) {
        ERR_POST(Error << "StyleManager::GetAtomStyle() got NULL object");
        return false;
    }
    
    // color by element for now
    const Residue::AtomInfo *info = object->graph->GetAtomInfo(atom);
    if (info->classification != Residue::eUnknownAtom) {
        atomStyle->style = StyleManager::eNotDisplayed;
        return true;
    }

    const Element *element = PeriodicTable.GetElement(info->atomicNumber);
    atomStyle->color = element->color;
    atomStyle->radius = 0.4;
    atomStyle->style = StyleManager::eSphereAtom;
    atomStyle->stacks = 3;
    atomStyle->slices = 6;
    atomStyle->name = info->glName;

	return true;
}

bool StyleManager::GetBondStyle(const Bond *bond,
        const AtomPntr& atom1, const AtomPntr& atom2,
        BondStyle *bondStyle)
{
    static const Vector magenta(1,0,1); 
    static const Vector green(0,1,0);

    const StructureObject *object;
    if (!bond->GetParentOfType(&object)) {
        ERR_POST(Error << "StyleManager::GetBondStyle() - can't get StructureObject parent");
        return false;
    }
    
/*
    // just do wireframe for now
    bondStyle->end1.style = bondStyle->end2.style = StyleManager::eLineBond;
*/

    if (bond->order == Bond::eVirtual) {
        // do worms for now
        bondStyle->end1.style = bondStyle->end2.style = StyleManager::eThickWormBond;

        // set tension, tighter for smaller protein alpha-helix
        const Molecule *molecule;
        if (!bond->GetParentOfType(&molecule)) {
            ERR_POST(Error << "StyleManager::GetBondStyle() - can't get Molecule parent");
            return false;
        }
        if (molecule->IsProtein())
            bondStyle->tension = -0.8;
        else
            bondStyle->tension = -0.4;
    }

    bondStyle->end1.segments = bondStyle->end2.segments = 4;
    bondStyle->end1.radius = bondStyle->end2.radius = 0.4;
    bondStyle->end1.atomCap = bondStyle->end2.atomCap = bondStyle->midCap = false;
    bondStyle->end1.sides = bondStyle->end2.sides = 6;

    const Residue *residue;
    bond->GetParentOfType(&residue);
    
    // color by element for now
    const Residue::AtomInfo *info = object->graph->GetAtomInfo(atom1);
    if (bond->order != Bond::eVirtual && (info->classification == Residue::ePartialBackboneAtom || 
        info->classification == Residue::eCompleteBackboneAtom)) {
        bondStyle->end1.style = bondStyle->end2.style = StyleManager::eNotDisplayed;
        return true;
    }
    const Element *element = PeriodicTable.GetElement(info->atomicNumber);
    bondStyle->end1.color = element->color;
    bondStyle->end1.name = info->glName;

    info = object->graph->GetAtomInfo(atom2);
    if (bond->order != Bond::eVirtual && (info->classification == Residue::ePartialBackboneAtom || 
        info->classification == Residue::eCompleteBackboneAtom)) {
        bondStyle->end1.style = bondStyle->end2.style = StyleManager::eNotDisplayed;
        return true;
    }
    element = PeriodicTable.GetElement(info->atomicNumber);
    bondStyle->end2.color = element->color;
    bondStyle->end2.name = info->glName;

    if (bond->order != Bond::eVirtual) {
        if (!residue || // can be NULL if this isn't an intra-residue bond
            residue->IsAminoAcid() || residue->IsNucleotide()) {
            bondStyle->end1.style = bondStyle->end2.style = StyleManager::eLineBond;
        } else {
            bondStyle->end1.style = bondStyle->end2.style = StyleManager::eCylinderBond;
            bondStyle->end1.radius = bondStyle->end2.radius = 0.2;
        } 
    } else {
        // color by master/slave object for now
        if (object->IsMaster())
            bondStyle->end1.color = bondStyle->end2.color = magenta;
        else
            bondStyle->end1.color = bondStyle->end2.color = green;
    }

    return true;
}

bool StyleManager::GetHelixStyle(const StructureObject *object,
    const Helix3D& helix, HelixStyle *helixStyle)
{
    helixStyle->style = StyleManager::eObjectWithArrow;
    helixStyle->radius = 1.8;
    helixStyle->color = Vector(0,1,0);
    helixStyle->sides = 12;
    helixStyle->arrowLength = 4.0;
    helixStyle->arrowBaseWidthProportion = 1.2;
    helixStyle->arrowTipWidthProportion = 0.4;
    return true;
}

bool StyleManager::GetStrandStyle(const StructureObject *object,
    const Strand3D& strand, StrandStyle *strandStyle)
{
    strandStyle->style = StyleManager::eObjectWithArrow;
    strandStyle->width = 2.0;
    strandStyle->thickness = 0.5;
    strandStyle->color = Vector(.7,.7,0);
    strandStyle->arrowLength = 2.8;
    strandStyle->arrowBaseWidthProportion = 1.6;
    return true;
}

END_SCOPE(Cn3D)

