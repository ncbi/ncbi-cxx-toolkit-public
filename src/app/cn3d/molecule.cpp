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
* Revision 1.25  2001/03/23 23:31:56  thiessen
* keep atom info around even if coords not all present; mainly for disulfide parsing in virtual models
*
* Revision 1.24  2001/03/23 04:18:52  thiessen
* parse and display disulfides
*
* Revision 1.23  2001/02/09 20:17:32  thiessen
* ignore atoms w/o alpha when doing structure realignment
*
* Revision 1.22  2001/02/08 23:01:50  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.21  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.20  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.19  2000/11/30 15:49:39  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.18  2000/11/13 18:06:53  thiessen
* working structure re-superpositioning
*
* Revision 1.17  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.16  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.15  2000/09/11 22:57:32  thiessen
* working highlighting
*
* Revision 1.14  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.13  2000/09/03 18:46:48  thiessen
* working generalized sequence viewer
*
* Revision 1.12  2000/08/28 23:47:18  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.11  2000/08/28 18:52:42  thiessen
* start unpacking alignments
*
* Revision 1.10  2000/08/27 18:52:21  thiessen
* extract sequence information
*
* Revision 1.9  2000/08/17 14:24:05  thiessen
* added working StyleManager
*
* Revision 1.8  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.7  2000/08/04 22:49:03  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.6  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.5  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
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
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>

#include "cn3d/molecule.hpp"
#include "cn3d/residue.hpp"
#include "cn3d/bond.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/atom_set.hpp"
#include "cn3d/chemical_graph.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

const int Molecule::VALUE_NOT_SET = -1;

Molecule::Molecule(ChemicalGraph *parentGraph,
    const CMolecule_graph& graph,
    const ResidueGraphList& standardDictionary,
    const ResidueGraphList& localDictionary) :
    StructureBase(parentGraph), type(eOther), sequence(NULL),
    gi(VALUE_NOT_SET), pdbChain(' '), nDomains(0)
{
    // get ID, name, and type
    id = graph.GetId().Get();
    CMolecule_graph::TDescr::const_iterator k, ke=graph.GetDescr().end();
    for (k=graph.GetDescr().begin(); k!=ke; k++) {
        if (k->GetObject().IsMolecule_type()) {
            type = static_cast<eType>(k->GetObject().GetMolecule_type());
        }
        if (k->GetObject().IsName()) {
            name = k->GetObject().GetName();
        }
    }

    // get Seq-id for biopolymer chains (for sequence alignment stuff)
    if (IsProtein() || IsNucleotide()) {
        if (graph.IsSetSeq_id()) {
            if (graph.GetSeq_id().IsGi())
                gi = graph.GetSeq_id().GetGi();
            else if (graph.GetSeq_id().IsPdb()) {
                pdbID = graph.GetSeq_id().GetPdb().GetMol().Get();
                if (graph.GetSeq_id().GetPdb().IsSetChain())
                    pdbChain = graph.GetSeq_id().GetPdb().GetChain();
            }
            else if (graph.GetSeq_id().IsLocal() &&
                graph.GetSeq_id().GetLocal().IsStr())
                pdbID = graph.GetSeq_id().GetLocal().GetStr();
        }
        if (gi == VALUE_NOT_SET && pdbID.size() == 0) {
            ERR_POST(Critical << "Molecule::Molecule() - biopolymer molecule, but can't get Seq-id");
            return;
        }
        // assign default identifier from parent, and assuming 'name' is actually the chainID
        if (pdbID.size() == 0) {
            const StructureObject *object;
            if (GetParentOfType(&object)) pdbID = object->pdbID;
            if (name.size() >= 1) pdbChain = name[0];
        }
    }

    // load residues from SEQUENCE OF Residue, storing virtual bonds along the way
    const Residue *prevResidue = NULL;
    const Bond *prevBond = NULL;
    CMolecule_graph::TInter_residue_bonds::const_iterator j, je;
    CMolecule_graph::TResidue_sequence::const_iterator i, ie=graph.GetResidue_sequence().end();
    for (i=graph.GetResidue_sequence().begin(); i!=ie; i++) {

        const Residue *residue = new Residue(this, (*i).GetObject(), id,
            standardDictionary, localDictionary);
        if (residues.find(residue->id) != residues.end())
            ERR_POST(Fatal << "confused by repeated Residue ID");
        residues[residue->id] = residue;

        // virtual bonds
        if (prevResidue && prevResidue->alphaID != Residue::NO_ALPHA_ID &&
            residue->alphaID != Residue::NO_ALPHA_ID) {

            // make sure there's a "real" inter-residue bond between these
            bool restarted = false, found = false;
            if (graph.IsSetInter_residue_bonds()) {
				j = graph.GetInter_residue_bonds().begin();
				je = graph.GetInter_residue_bonds().end();
                do {
                    if (j == je) {
                        if (!restarted) {
                            j = graph.GetInter_residue_bonds().begin();
                            restarted = true;
                        } else
                        break;
                    }
                    if ((j->GetObject().GetAtom_id_1().GetResidue_id().Get() == prevResidue->id &&
                         j->GetObject().GetAtom_id_2().GetResidue_id().Get() == residue->id) ||
                        (j->GetObject().GetAtom_id_2().GetResidue_id().Get() == prevResidue->id &&
                         j->GetObject().GetAtom_id_1().GetResidue_id().Get() == residue->id)) {
                        found = true;
                    }
                    j++;
                } while (!found);
            }

            if (found) {
                const Bond *bond = MakeBond(this,
                    id, prevResidue->id, prevResidue->alphaID,
                    id, residue->id, residue->alphaID,
                    Bond::eVirtual);
                if (bond) {
                    interResidueBonds.push_back(bond);
                    if (prevBond) {
                        (const_cast<Bond *>(prevBond))->nextVirtual = bond;
                        (const_cast<Bond *>(bond))->previousVirtual = prevBond;
                    }
                }
                prevBond = bond;
            } else
                prevBond = NULL;
        } else
            prevBond = NULL;
        prevResidue = residue;
    }

    residueDomains.resize(residues.size(), VALUE_NOT_SET);
    // keep s.s. maps only for protein chains
    if (IsProtein()) residueSecondaryStructures.resize(residues.size(), eCoil);

    // load inter-residue bonds from SEQUENCE OF Inter-residue-bond OPTIONAL
    if (graph.IsSetInter_residue_bonds()) {
        je = graph.GetInter_residue_bonds().end();
        for (j=graph.GetInter_residue_bonds().begin(); j!=je; j++) {

            int order = j->GetObject().IsSetBond_order() ?
                j->GetObject().GetBond_order() : Bond::eUnknown;
            const Bond *bond = MakeBond(this,
                j->GetObject().GetAtom_id_1(),
                j->GetObject().GetAtom_id_2(),
                order);
            if (bond) interResidueBonds.push_back(bond);

            // add to disulfide map if virtual disulfide added or this bond is flagged as disulfide
            if (parentGraph->CheckForDisulfide(this,
                    j->GetObject().GetAtom_id_1(), j->GetObject().GetAtom_id_2(),
                    &interResidueBonds, const_cast<Bond*>(bond), this) ||
                (bond && bond->order == Bond::eRealDisulfide)) {
                disulfideMap[j->GetObject().GetAtom_id_1().GetResidue_id().Get()] =
                    j->GetObject().GetAtom_id_2().GetResidue_id().Get();
                disulfideMap[j->GetObject().GetAtom_id_2().GetResidue_id().Get()] =
                    j->GetObject().GetAtom_id_1().GetResidue_id().Get();
            }
        }
    }
}

Vector Molecule::GetResidueColor(int sequenceIndex) const
{
    static Vector gray(.5,.5,.5);

    // this assumes that the "index" - the position of the residue in the sequence,
    // starting from zero, is always one less than the residueID from the ASN1
    // data, which starts from one.
    ResidueMap::const_iterator r = residues.find(sequenceIndex + 1);

    if (r == residues.end()) return gray;
    const Residue *residue = r->second;

    // if no known alpha atom, just use gray
    if (residue->alphaID == Residue::NO_ALPHA_ID) return gray;

    AtomStyle style;
    AtomPntr atom(id, residue->id, residue->alphaID);
    if (!parentSet->styleManager->GetAtomStyle(residue, atom, &style)) return gray;
    return style.color;
}

bool Molecule::GetAlphaCoords(int nResidues, const int *seqIndexes, const Vector * *coords) const
{
    const StructureObject *object;
    if (!GetParentOfType(&object)) return false;
    if (object->coordSets.size() != 1) {
        ERR_POST("Can't align structures with multiple CoordSets");
        return false;
    }

    for (int i=0; i<nResidues; i++) {

        int rID = seqIndexes[i] + 1;    // residueIDs start at 1
        ResidueMap::const_iterator r = residues.find(rID);
        if (r == residues.end()) {
            ERR_POST(Error << "Can't find residueID " << rID
                << " in " << pdbID << " chain '" << (char) pdbChain << "'");
            return false;
        }

        int aID = (r->second->alphaID);
        if (aID == Residue::NO_ALPHA_ID) {
            ERR_POST(Warning << "No alpha atom in residueID " << rID
                << " from " << pdbID << " chain '" << (char) pdbChain << "'");
            coords[i] = NULL;
            continue;
        }

        AtomPntr atom(id, rID, aID);
        const AtomCoord* atomCoord = object->coordSets.front()->atomSet->GetAtom(atom);
        if (!atomCoord) {
            ERR_POST(Warning << "Can't get AtomCoord for (m"
                << id << ",r" << rID << ",a" << aID << ")");
            coords[i] = NULL;
            continue;
        }

        coords[i] = &(atomCoord->site);
    }

    return true;
}

std::string Molecule::GetTitle(void) const
{
    std::string str;
    str = pdbID;
    if (pdbChain != ' ') {
        str += '_';
        str += (char) pdbChain;
    }
    return str;
}

END_SCOPE(Cn3D)

