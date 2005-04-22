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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>

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
    StructureBase(parentGraph), type(eOther), sequence(NULL), nDomains(0)
{
    int gi = MoleculeIdentifier::VALUE_NOT_SET, pdbChain = MoleculeIdentifier::VALUE_NOT_SET;
    string pdbID, accession;

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
    if (IsProtein() || IsNucleotide()) {
        if (graph.IsSetSeq_id()) {
            if (graph.GetSeq_id().IsGi())
                gi = graph.GetSeq_id().GetGi();
            else if (graph.GetSeq_id().IsPdb()) {
                pdbID = graph.GetSeq_id().GetPdb().GetMol().Get();
                if (graph.GetSeq_id().GetPdb().IsSetChain())
                    pdbChain = graph.GetSeq_id().GetPdb().GetChain();
                else
                    pdbChain = ' ';
            }
            else if (graph.GetSeq_id().IsLocal() && graph.GetSeq_id().GetLocal().IsStr()) {
                accession = graph.GetSeq_id().GetLocal().GetStr();
                // special case where local accession is actually a PDB chain + extra stuff
                if (pdbID.size() == 0 && accession.size() >= 7 &&
                        accession[4] == ' ' && accession[6] == ' ' && isalpha(accession[5])) {
                    pdbID = accession.substr(0, 4);
                    pdbChain = accession[5];
                    accession.erase();
                }
            }
        }
        if (gi == MoleculeIdentifier::VALUE_NOT_SET && pdbID.size() == 0 && accession.size() == 0) {
            ERRORMSG("Molecule::Molecule() - biopolymer molecule, but can't get Seq-id");
            return;
        }
    }

    // if no PDB id assigned to biopolymer, assign default PDB identifier from parent, assuming 'name'
    // is actually the chainID if this is a biopolymer
    const StructureObject *object;
    if (!GetParentOfType(&object)) return;
    if ((IsProtein() || IsNucleotide()) && pdbID.size() == 0) {
        pdbID = object->pdbID;
        if (name.size() == 1) pdbChain = name[0];
    }

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
    identifier = MoleculeIdentifier::GetIdentifier(this, pdbID, pdbChain, gi, accession);
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
            ERRORMSG("Can't find residueID " << rID
                << " in " << identifier->pdbID << " chain '"
                << (char) identifier->pdbChain << "'");
            return -1;
        }

        int aID = (r->second->alphaID);
        if (aID == Residue::NO_ALPHA_ID) {
            WARNINGMSG("No alpha atom in residueID " << rID
                << " from " << identifier->pdbID << " chain '"
                << (char) identifier->pdbChain << "'");
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
                if (identifier->pdbChain != MoleculeIdentifier::VALUE_NOT_SET && identifier->pdbChain != ' ')
                    oss << " (" << (char) identifier->pdbChain << ')';
                oss << '\0';

                // draw label
                string labelText = oss.str();
                delete oss.str();
                parentSet->renderer->DrawLabel(labelText, labelPosition, labelColor);
            }
        }
    }

    return true;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.45  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.44  2005/03/15 18:53:49  thiessen
* don't draw single-residue heterogens in aa/nuc style
*
* Revision 1.43  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.42  2004/05/20 18:49:21  thiessen
* don't do structure realignment if < 3 coords present
*
* Revision 1.41  2004/03/15 17:34:03  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.40  2004/02/19 17:04:57  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.39  2003/06/21 08:18:58  thiessen
* show all atoms with coordinates, even if not in all coord sets
*
* Revision 1.38  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.37  2002/04/19 17:34:03  thiessen
* fix for alpha-only nucleotides
*
* Revision 1.36  2002/02/01 13:55:31  thiessen
* fix labeling bug when domain hidden
*
* Revision 1.35  2002/02/01 00:41:21  thiessen
* tweaks
*
* Revision 1.34  2002/01/24 20:08:16  thiessen
* fix local id problem
*
* Revision 1.33  2001/12/12 14:04:13  thiessen
* add missing object headers after object loader change
*
* Revision 1.32  2001/10/16 21:49:07  thiessen
* restructure MultiTextDialog; allow virtual bonds for alpha-only PDB's
*
* Revision 1.31  2001/08/24 00:41:35  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.30  2001/08/21 01:10:45  thiessen
* add labeling
*
* Revision 1.29  2001/08/09 19:07:13  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.28  2001/07/16 15:35:37  thiessen
* fix unaligned chain identifier ommission
*
* Revision 1.27  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.26  2001/04/04 00:27:14  thiessen
* major update - add merging, threader GUI controls
*
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
*/
