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
*      Classes to hold residues
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/07/11 13:45:31  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#include <objects/mmdb1/Residue_graph_pntr.hpp>
#include <objects/mmdb1/Biost_resid_graph_set_pntr.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/mmdb1/Biomol_descr.hpp>
#include <objects/mmdb1/Intra_residue_bond.hpp>
#include <objects/mmdb1/Atom.hpp>

#include "cn3d/residue.hpp"
#include "cn3d/bond.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

const char Residue::NO_CODE = '?';

Residue::Residue(StructureBase *parent,
    const CResidue& residue, int moleculeID,
    const ResidueGraphList& standardDictionary,
    const ResidueGraphList& localDictionary) :
    StructureBase(parent), code(NO_CODE)
{
    // get ID
    id = residue.GetId().Get();

    // standard (of correct type) or local dictionary?
    const ResidueGraphList *dictionary;
    int graphID;
    if (residue.GetResidue_graph().IsStandard() &&
        residue.GetResidue_graph().GetStandard().GetBiostruc_residue_graph_set_id().IsOther_database() &&
        residue.GetResidue_graph().GetStandard().GetBiostruc_residue_graph_set_id().GetOther_database().GetDb() == "Standard residue dictionary" &&
        residue.GetResidue_graph().GetStandard().GetBiostruc_residue_graph_set_id().GetOther_database().GetTag().IsId() &&
        residue.GetResidue_graph().GetStandard().GetBiostruc_residue_graph_set_id().GetOther_database().GetTag().GetId() == 1) {
        dictionary = &standardDictionary;
        graphID = residue.GetResidue_graph().GetStandard().GetResidue_graph_id().Get();
    } else if (residue.GetResidue_graph().IsLocal()) {
        dictionary = &localDictionary;
        graphID = residue.GetResidue_graph().GetLocal().Get();
    } else
        ERR_POST(Fatal << "confused by Molecule #?, Residue #" << id << "; can't find appropriate dictionary");

    // look up appropriate Residue_graph
    CResidue_graph *residueGraph = NULL;
    ResidueGraphList::const_iterator i, ie=dictionary->end();
    for (i=dictionary->begin(); i!=ie; i++) {
        if ((*i).GetObject().GetId().Get() == graphID) {
            residueGraph = (*i).GetPointer();
            break;
        }
    }
    if (!residueGraph) 
        ERR_POST(Fatal << "confused by Molecule #?, Residue #" << id 
                       << "; can't find Residue-graph #" << graphID);

    // get iupac-code if present - assume it's the first character of the first VisibleString
    if (residueGraph->IsSetIupac_code())
        code = residueGraph->GetIupac_code().front()[0];

    // get name if present
    if (residueGraph->IsSetDescr()) {
        CResidue_graph::TDescr::const_iterator j, je = residueGraph->GetDescr().end();
        for (j=residueGraph->GetDescr().begin(); j!=je; j++) {
            if ((*j).GetObject().IsName()) {
                name = (*j).GetObject().GetName();
                break;
            }
        }
    }

    // get bonds
    CResidue_graph::TBonds::const_iterator b, be = residueGraph->GetBonds().end();
    for (b=residueGraph->GetBonds().begin(); b!=be; b++) {
        int order = (*b).GetObject().IsSetBond_order() ? 
                (*b).GetObject().GetBond_order() : Bond::eUnknown;
        Bond *bond = new Bond(this,
            moleculeID, id, (*b).GetObject().GetAtom_id_1().Get(),
            moleculeID, id, (*b).GetObject().GetAtom_id_2().Get(),
            order);
        bonds.push_back(bond);
    }

    // get atom info
    CResidue_graph::TAtoms::const_iterator a, ae = residueGraph->GetAtoms().end();
    for (a=residueGraph->GetAtoms().begin(); a!=ae; a++) {
        const CAtom& atom = (*a).GetObject();
        AtomInfo *info = new AtomInfo;
        // get name if present
        if (atom.IsSetName()) info->name = atom.GetName();
        // get code if present - just use first one of the SEQUENCE
        if (atom.IsSetIupac_code())
            info->code = atom.GetIupac_code().front();
        // get atomic number, assuming it's the integer value of the enumerated type
        info->atomicNumber = static_cast<int>(atom.GetElement());
        // get ionizable status
        if (atom.IsSetIonizable_proton() &&
            atom.GetIonizable_proton() == CAtom::eIonizable_proton_true)
            info->isIonizableProton = true;
        else
            info->isIonizableProton = false;

        // add info to map
        if (atomInfos.find(atom.GetId().Get()) != atomInfos.end())
            ERR_POST(Fatal << "Residue #" << id << ": confused by multiple atom IDs " << atom.GetId().Get());
        atomInfos[atom.GetId().Get()] = info;
    }
}

Residue::~Residue(void)
{
    AtomInfoMap::iterator a, ae = atomInfos.end();
    for (a=atomInfos.begin(); a!=ae; a++) delete (*a).second;
}

bool Residue::Draw(void) const
{
    // prune draw tree here
    TESTMSG("not drawing Residue #" << id << " yet");
    return false;
}

END_SCOPE(Cn3D)

