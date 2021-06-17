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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>

#include "remove_header_conflicts.hpp"

#include "molecule.hpp"
#include "residue.hpp"
#include "bond.hpp"
#include "style_manager.hpp"
#include "structure_set.hpp"
#include "coord_set.hpp"
#include "atom_set.hpp"
#include "chemical_graph.hpp"
#include "molecule_identifier.hpp"
#include "show_hide_manager.hpp"
#include "cn3d_tools.hpp"
#include "opengl_renderer.hpp"
#include "cn3d_colors.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

const int Molecule::NO_DOMAIN_SET = -1;

Molecule::Molecule(ChemicalGraph *parentGraph,
    const CMolecule_graph& graph,
    const ResidueGraphList& standardDictionary,
    const ResidueGraphList& localDictionary) :
        StructureBase(parentGraph), type(eOther), nDomains(0), sequence(NULL)
{
    // get ID, name, and type
    id = graph.GetId().Get();
    CMolecule_graph::TDescr::const_iterator k, ke=graph.GetDescr().end();
    for (k=graph.GetDescr().begin(); k!=ke; ++k) {
        if (k->GetObject().IsMolecule_type()) {
            type = static_cast<eType>(k->GetObject().GetMolecule_type());
        }
        if (k->GetObject().IsName()) {
            name = k->GetObject().GetName();
        }
    }

    // get Seq-id for biopolymer chains (for sequence alignment stuff)
    MoleculeIdentifier::SeqIdList seqIDs;
    if (IsProtein() || IsNucleotide()) {
        if (graph.IsSetSeq_id()) {
            seqIDs.push_back(CRef<CSeq_id>(const_cast<CSeq_id*>(&(graph.GetSeq_id()))));
        } else {
            ERRORMSG("Molecule::Molecule() - biopolymer molecule, but can't get Seq-id");
            return;
        }
    }

    const StructureObject *object;
    if (!GetParentOfType(&object)) return;

    // load residues from SEQUENCE OF Residue, storing virtual bonds along the way
    CMolecule_graph::TInter_residue_bonds::const_iterator j, je, jOrig;
    if (graph.IsSetInter_residue_bonds()) {
        j = graph.GetInter_residue_bonds().begin();
        je = graph.GetInter_residue_bonds().end();
    }
    const Residue *prevResidue = NULL;
    const Bond *prevBond = NULL;
    CMolecule_graph::TResidue_sequence::const_iterator i, ie=graph.GetResidue_sequence().end();
    int nResidues = 0;

    for (i=graph.GetResidue_sequence().begin(); i!=ie; ++i) {

        const Residue *residue = new Residue(this, (*i).GetObject(), id,
            standardDictionary, localDictionary, graph.GetResidue_sequence().size(), type);
        residues[residue->id] = residue;
        ++nResidues;

        // this assumption is frequently made elsewhere, relating seqLocs (numbering from zero)
        // to residue ID's (numbering from one) - so enforce it here
        if (residue->id != nResidues)
            ERRORMSG("Residue ID's must be ordered consecutively starting with one");

        // virtual bonds between successive alphas
        if (prevResidue && prevResidue->alphaID != Residue::NO_ALPHA_ID &&
            residue->alphaID != Residue::NO_ALPHA_ID) {

            bool foundReal = false;

            // see if there's a "real" inter-residue bond between these in the chemical graph
            if (graph.IsSetInter_residue_bonds()) {
                jOrig = j;
                do {
                    if ((j->GetObject().GetAtom_id_1().GetResidue_id().Get() == prevResidue->id &&
                         j->GetObject().GetAtom_id_2().GetResidue_id().Get() == residue->id) ||
                        (j->GetObject().GetAtom_id_2().GetResidue_id().Get() == prevResidue->id &&
                         j->GetObject().GetAtom_id_1().GetResidue_id().Get() == residue->id))
                        foundReal = true;
                    if (++j == je) j = graph.GetInter_residue_bonds().begin();
                } while (!foundReal && j != jOrig);
            }

            // for alpha only models, there are no inter-residue bonds in the
            // chemical graph, so check inter-atomic distances for cases where either one
            // of the two residues is alpha only.
            if (!foundReal && (IsProtein() || IsNucleotide()) &&
                (residue->NAtomsWithAnyCoords() == 1 || prevResidue->NAtomsWithAnyCoords() == 1)) {

                // get atom coordinates
                AtomPntr ap1(id, residue->id, residue->alphaID);
                const AtomCoord* atom1 = object->coordSets.front()->atomSet->GetAtom(ap1, true, true);
                AtomPntr ap2(id, prevResidue->id, prevResidue->alphaID);
                const AtomCoord* atom2 = object->coordSets.front()->atomSet->GetAtom(ap2, true, true);

                // check distance - ok if <= 5.0 Angstroms (or 10.0 for nucleotides)
                if (atom1 && atom2 &&
                        (atom1->site - atom2->site).length() <=
                            (IsProtein() ? 5.0 : 10.0))
                    foundReal = true;
            }

            if (foundReal) {
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

    residueDomains.resize(residues.size(), NO_DOMAIN_SET);

    // keep s.s. maps only for protein chains
    if (IsProtein()) residueSecondaryStructures.resize(residues.size(), eCoil);

    // load inter-residue bonds from SEQUENCE OF Inter-residue-bond OPTIONAL
    if (graph.IsSetInter_residue_bonds()) {
        je = graph.GetInter_residue_bonds().end();
        for (j=graph.GetInter_residue_bonds().begin(); j!=je; ++j) {

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

    // get identifier
    identifier = MoleculeIdentifier::GetIdentifier(this, seqIDs);
}

Vector Molecule::GetResidueColor(int sequenceIndex) const
{
    static const Vector gray(.5,.5,.5);

    // this assumes that the "index" - the position of the residue in the sequence,
    // starting from zero, is always one less than the residueID from the ASN1
    // data, which starts from one.
    ResidueMap::const_iterator r = residues.find(sequenceIndex + 1);

    if (r == residues.end()) return gray;
    const Residue *residue = r->second;

    // if no known alpha atom, just use gray
    if (residue->alphaID == Residue::NO_ALPHA_ID) return gray;
    AtomPntr atom(id, residue->id, residue->alphaID);

    // just use the first AtomSet (e.g. first model) of this object
    const StructureObject *object;
    if (!GetParentOfType(&object)) return gray;
    const AtomCoord *atomCoord = object->coordSets.front()->atomSet->
        GetAtom(atom, parentSet->showHideManager->OverlayConfEnsembles(), true);
    if (!atomCoord) return gray;

    AtomStyle style;
    if (!parentSet->styleManager->GetAtomStyle(residue, atom, atomCoord, &style)) return gray;
    return style.color;
}

int Molecule::GetAlphaCoords(int nResidues, const int *seqIndexes, const Vector * *coords) const
{
    const StructureObject *object;
    if (!GetParentOfType(&object)) return false;
    if (object->coordSets.size() != 1) {
        ERRORMSG("Can't align structures with multiple CoordSets");
        return -1;
    }

    int nCoords = 0;
    for (int i=0; i<nResidues; ++i) {

        coords[i] = NULL;

        if (seqIndexes[i] < 0)
            continue;
        int rID = seqIndexes[i] + 1;    // residueIDs start at 1

        ResidueMap::const_iterator r = residues.find(rID);
        if (r == residues.end()) {
			#ifdef _STRUCTURE_USE_LONG_PDB_CHAINS_
				ERRORMSG("Can't find residueID " << rID
					<< " in " << identifier->pdbID << " chain '"
					<< identifier->pdbChain << "'");
			#else
				ERRORMSG("Can't find residueID " << rID
					<< " in " << identifier->pdbID << " chain '"
					<< (char) identifier->pdbChain << "'");
			#endif

            return -1;
        }

        int aID = (r->second->alphaID);
        if (aID == Residue::NO_ALPHA_ID) {
			#ifdef _STRUCTURE_USE_LONG_PDB_CHAINS_
				WARNINGMSG("No alpha atom in residueID " << rID
					<< " from " << identifier->pdbID << " chain '"
					<< identifier->pdbChain << "'");
			#else
				WARNINGMSG("No alpha atom in residueID " << rID
					<< " from " << identifier->pdbID << " chain '"
					<< (char) identifier->pdbChain << "'");
			#endif

            continue;
        }

        AtomPntr atom(id, rID, aID);
        const AtomCoord* atomCoord = object->coordSets.front()->atomSet->GetAtom(atom);
        if (!atomCoord) {
            WARNINGMSG("Can't get AtomCoord for (m"
                << id << ",r" << rID << ",a" << aID << ")");
            continue;
        }

        coords[i] = &(atomCoord->site);
        ++nCoords;
    }

    return nCoords;
}

bool Molecule::DrawAllWithTerminiLabels(const AtomSet *atomSet) const
{
    // draw regular objects
    if (!DrawAll(atomSet)) return false;

    // add termini labels
    if ((IsProtein() || IsNucleotide()) && NResidues() >= 2 && parentSet->showHideManager->IsVisible(this)) {

        const StyleSettings& settings = parentSet->styleManager->GetGlobalStyle();
        if ((IsProtein() && settings.proteinLabels.terminiOn) ||
            (IsNucleotide() && settings.nucleotideLabels.terminiOn)) {

            // try to color labels to contrast with background
            static const Vector white(1,1,1), black(0,0,0);
            const Vector& labelColor =
                Colors::IsLightColor(settings.backgroundColor) ? black : white;

            // do start (N or 5') and end (C or 3') labels
            for (int startTerminus=1; startTerminus>=0; --startTerminus) {

                // determine color and location - assumes sequential residue id's (from 1)
                const Vector *alphaPos = NULL, *prevPos = NULL;
                int res = startTerminus ? 1 : residues.size(),
                    resEnd = startTerminus ? residues.size() : 1,
                    resInc = startTerminus ? 1 : -1;

                // find coordinates of two terminal alpha atoms
                const Residue *termRes = NULL;
                for (; res!=resEnd; res+=resInc) {
                    const Residue *residue = residues.find(res)->second;
                    if (residue->alphaID != Residue::NO_ALPHA_ID) {
                        AtomPntr ap(id, res, residue->alphaID);
                        const AtomCoord *atom =
                            atomSet->GetAtom(ap, parentSet->showHideManager->OverlayConfEnsembles(), true);
                        if (atom) {
                            if (!alphaPos) {
                                alphaPos = &(atom->site);
                                termRes = residue;
                            } else if (!prevPos) {
                                prevPos = &(atom->site);
                                break;
                            }
                        }
                    }
                }
                if (!(alphaPos && prevPos)) {
                    WARNINGMSG("Molecule::DrawAllWithTerminiLabels() - "
                        << "can't get two terminal alpha coords");
                    continue;
                }
                if (!parentSet->showHideManager->IsVisible(termRes)) continue;
                Vector labelPosition = *alphaPos + 0.5 * (*alphaPos - *prevPos);

                // determine label text
                CNcbiOstrstream oss;
                if (IsProtein()) {
                    if (startTerminus)
                        oss << "N";
                    else
                        oss << "C";
                } else {
                    if (startTerminus)
                        oss << "5'";
                    else
                        oss << "3'";
                }

				#ifdef _STRUCTURE_USE_LONG_PDB_CHAINS_
					if (!identifier->pdbChain.empty() && identifier->pdbChain != " ")
						oss << " (" << identifier->pdbChain << ')';
				#else
					if (identifier->pdbChain != MoleculeIdentifier::VALUE_NOT_SET && identifier->pdbChain != ' ')
						oss << " (" << (char) identifier->pdbChain << ')';
				#endif

                // draw label
                string labelText = (string) CNcbiOstrstreamToString(oss);
                parentSet->renderer->DrawLabel(labelText, labelPosition, labelColor);
            }
        }
    }

    return true;
}

END_SCOPE(Cn3D)
