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
*      Classes to hold molecules
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2000/07/18 02:41:33  thiessen
* fix bug in virtual bonds and altConfs
*
* Revision 1.3  2000/07/17 04:20:49  thiessen
* now does correct structure alignment transformation
*
* Revision 1.2  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
* Revision 1.1  2000/07/11 13:45:30  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Molecule_id.hpp>

#include "cn3d/molecule.hpp"
#include "cn3d/residue.hpp"
#include "cn3d/bond.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

Molecule::Molecule(StructureBase *parent,
    const CMolecule_graph& graph,
    const ResidueGraphList& standardDictionary,
    const ResidueGraphList& localDictionary) :
    StructureBase(parent), type(eOther)
{
    // get ID and type
    id = graph.GetId().Get();
    CMolecule_graph::TDescr::const_iterator k, ke=graph.GetDescr().end();
    for (k=graph.GetDescr().begin(); k!=ke; k++) {
        if (k->GetObject().IsMolecule_type()) {
            type = static_cast<eType>(k->GetObject().GetMolecule_type());
            break;
        }
    }

    // load residues from SEQUENCE OF Residue, storing virtual bonds along the way
    Residue *prevResidue = NULL;
    int alphaID1, alphaID2;
    CMolecule_graph::TResidue_sequence::const_iterator i, ie=graph.GetResidue_sequence().end();
    for (i=graph.GetResidue_sequence().begin(); i!=ie; i++) {

        Residue *residue = new Residue(this, (*i).GetObject(), id,
            standardDictionary, localDictionary);
        if (residues.find(residue->id) != residues.end())
            ERR_POST(Fatal << "confused by repeated Residue ID");
        residues[residue->id] = residue;

        if (prevResidue) {
            // virtual bonds
            alphaID1 = prevResidue->alphaID;
            if (alphaID1 == Residue::NO_ALPHA_ID) continue;
            alphaID2 = residue->alphaID;
            if (alphaID2 == Residue::NO_ALPHA_ID) continue;
            const Bond *bond = MakeBond(this, 
                id, prevResidue->id, alphaID1,
                id, residue->id, alphaID2,
                Bond::eSingle);
            if (bond) virtualBonds.push_back(bond);
        }
        prevResidue = residue;
    }

    // load inter-residue bonds from SEQUENCE OF Inter-residue-bond OPTIONAL
    if (graph.IsSetInter_residue_bonds()) {
        CMolecule_graph::TInter_residue_bonds::const_iterator j, je=graph.GetInter_residue_bonds().end();
        for (j=graph.GetInter_residue_bonds().begin(); j!=je; j++) {
            
            int order = j->GetObject().IsSetBond_order() ? 
                j->GetObject().GetBond_order() : Bond::eUnknown;
            const Bond *bond = MakeBond(this, 
                j->GetObject().GetAtom_id_1(), 
                j->GetObject().GetAtom_id_2(),
                order);
            if (bond) interResidueBonds.push_back(bond);
        }
    }
}

//bool Molecule::Draw(const StructureBase *data) const
//{
//    TESTMSG("drawing Molecule #" << id);
//    return true;
//}

END_SCOPE(Cn3D)

