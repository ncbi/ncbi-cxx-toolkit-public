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
    const Element *element = PeriodicTable.GetElement(info->atomicNumber);
    atomStyle->color = element->color;
    atomStyle->radius = 0.4;
    atomStyle->style = StyleManager::eSphereAtom;
    atomStyle->stacks = 3;
    atomStyle->slices = 6;

	return true;
}

bool StyleManager::GetBondStyle(const StructureObject *object,
        const AtomPntr& atom1, const AtomPntr& atom2,
        BondStyle *bondStyle)
{
    static const Vector magenta(1,0,1); 
    static const Vector green(0,1,0);

    if (!object) {
        ERR_POST(Error << "StyleManager::GetBondStyle() got NULL object");
        return false;
    }
    
/*
    // just do wireframe for now
    bondStyle->end1.style = bondStyle->end2.style = StyleManager::eLineBond;
*/

    // do cylinders for now
    bondStyle->end1.style = bondStyle->end2.style = StyleManager::eCylinderBond;
    bondStyle->end1.radius = bondStyle->end2.radius = 0.2;
    bondStyle->end1.atomCap = bondStyle->end2.atomCap = bondStyle->midCap = false;
    bondStyle->end1.sides = bondStyle->end2.sides = 6;

/*
    // color by master/slave object for now
    if (object->IsMaster())
        bondStyle->end1.color = bondStyle->end2.color = magenta;
    else
        bondStyle->end1.color = bondStyle->end2.color = green;
*/

    // color by element for now
    const Residue::AtomInfo *info = object->graph->GetAtomInfo(atom1);
    const Element *element = PeriodicTable.GetElement(info->atomicNumber);
    bondStyle->end1.color = element->color;

    info = object->graph->GetAtomInfo(atom2);
    element = PeriodicTable.GetElement(info->atomicNumber);
    bondStyle->end2.color = element->color;

    return true;
}

END_SCOPE(Cn3D)

