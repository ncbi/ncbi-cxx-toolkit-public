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
*      Classes to hold chemical bonds
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/07/16 23:19:10  thiessen
* redo of drawing system
*
* Revision 1.1  2000/07/11 13:45:28  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb1/Atom_id.hpp>

#include "cn3d/bond.hpp"
#include "cn3d/atom_set.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/periodic_table.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

const Bond* MakeBond(StructureBase *parent,
    int mID1, int rID1, int aID1, 
    int mID2, int rID2, int aID2, 
    int bondOrder)
{
    // get StructureObject* parent
    const StructureObject *object;
    if (!parent->GetParentOfType(&object) || object->coordSets.size() == 0) {
        ERR_POST("Bond() : parent doesn't have any CoordSets");
        return NULL;
    }

    // check for presence of both atoms
    StructureObject::CoordSetList::const_iterator c, ce=object->coordSets.end();
    for (c=object->coordSets.begin(); c!=ce; c++) {
        if (!((*c)->atomSet->GetAtom(mID1, rID1, aID1, true, true)) ||
            !((*c)->atomSet->GetAtom(mID2, rID2, aID2, true, true))) 
            break;
    }
    if (c != ce) return NULL;

    Bond *bond = new Bond(parent);
    bond->atom1.mID = mID1;
    bond->atom1.rID = rID1;
    bond->atom1.aID = aID1;
    bond->atom2.mID = mID2;
    bond->atom2.rID = rID2;
    bond->atom2.aID = aID2;
    bond->order = static_cast<Bond::eBondOrder>(bondOrder);
    return bond;
}

const Bond* MakeBond(StructureBase *parent,
    const CAtom_pntr& atomPtr1, const CAtom_pntr& atomPtr2, int bondOrder)
{
    return MakeBond(parent,
        atomPtr1.GetMolecule_id().Get(),
        atomPtr1.GetResidue_id().Get(),
        atomPtr1.GetAtom_id().Get(),
        atomPtr2.GetMolecule_id().Get(),
        atomPtr2.GetResidue_id().Get(),
        atomPtr2.GetAtom_id().Get(),
        bondOrder
    );
}

bool Bond::Draw(const StructureBase *data) const
{
    // at this point 'data' should be an AtomSet*
    const AtomSet *atomSet;
    if ((atomSet = dynamic_cast<const AtomSet *>(data)) == NULL) {
        ERR_POST(Error << "Bond::Draw(data) - data not AtomSet*");
        return false;
    }

    // get OpenGLRenderer
    const StructureSet *set;
    if (!GetParentOfType(&set) || !set->renderer) {
        ERR_POST(Warning << "Bond::Draw() - no renderer");
        return false;
    }

    // get Atom* for appropriate altConf
    static bool overlayEnsembles = true;
    const Atom *a1 = atomSet->GetAtom(atom1.mID, atom1.rID, atom1.aID, overlayEnsembles);
    if (!a1) return true;
    const Atom *a2 = atomSet->GetAtom(atom2.mID, atom2.rID, atom2.aID, overlayEnsembles);
    if (!a2) return true;

    // draw line
    if (atom1.mID != atom2.mID) {
        static const Vector connectionColor(1,1,0);
        set->renderer->DrawLine(a1->site, a2->site, connectionColor, connectionColor);

    } else {
        const ChemicalGraph *graph;
        if (!GetParentOfType(&graph)) {
            ERR_POST(Warning << "Bond::Draw() - can't get ChemicalGraph parent");
            return false;
        }
        const Residue::AtomInfo *info = graph->GetAtomInfo(atom1.mID, atom1.rID, atom1.aID);
        const Element *element1 = PeriodicTable.GetElement(info->atomicNumber);
        info = graph->GetAtomInfo(atom2.mID, atom2.rID, atom2.aID);
        const Element *element2 = PeriodicTable.GetElement(info->atomicNumber);
        set->renderer->DrawLine(a1->site, a2->site, element1->color, element2->color);
    }

    return true;
}

END_SCOPE(Cn3D)

