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
* Revision 1.22  2001/05/15 23:48:37  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.21  2001/04/18 15:46:53  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.20  2001/03/23 23:31:56  thiessen
* keep atom info around even if coords not all present; mainly for disulfide parsing in virtual models
*
* Revision 1.19  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.18  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.17  2000/11/30 15:49:39  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.16  2000/09/11 22:57:32  thiessen
* working highlighting
*
* Revision 1.15  2000/08/25 14:22:00  thiessen
* minor tweaks
*
* Revision 1.14  2000/08/19 02:59:05  thiessen
* fix tranparent sphere bug
*
* Revision 1.13  2000/08/18 18:57:39  thiessen
* added transparent spheres
*
* Revision 1.12  2000/08/17 14:24:06  thiessen
* added working StyleManager
*
* Revision 1.11  2000/08/13 02:43:01  thiessen
* added helix and strand objects
*
* Revision 1.10  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.9  2000/08/07 00:21:18  thiessen
* add display list mechanism
*
* Revision 1.8  2000/08/04 22:49:03  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.7  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.6  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.5  2000/07/17 22:37:18  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.4  2000/07/17 11:59:16  thiessen
* fix nucleotide virtual bonds
*
* Revision 1.3  2000/07/17 04:20:50  thiessen
* now does correct structure alignment transformation
*
* Revision 1.2  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
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
#include "cn3d/structure_set.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/atom_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/periodic_table.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/cn3d_colors.hpp"
#include "cn3d/show_hide_manager.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

const char Residue::NO_CODE = '?';
const int Residue::NO_ALPHA_ID = -1;

static Residue::eAtomClassification ClassifyAtom(const Residue *residue, const CAtom& atom)
{
    if (!atom.IsSetIupac_code()) return Residue::eUnknownAtom;
    std::string code = atom.GetIupac_code().front();
    CAtom::EElement element = atom.GetElement();

    if (residue->IsAminoAcid()) {

        // amino acid C-alpha
        if (element==CAtom::eElement_c && code==" CA ")
            return Residue::eAlphaBackboneAtom;

        // amino acid partial backbone
        if (
            (element==CAtom::eElement_c && code==" C  ") ||
            (element==CAtom::eElement_n && code==" N  ")
           )
            return Residue::ePartialBackboneAtom;

        // amino acid complete backbone (all backbone that's not part of "partial")
        // including both GLY alpha hydrogens and terminal COOH and NH3+
        if (
            (element==CAtom::eElement_o &&
                (code==" O  " || code==" OXT")) ||
            (element==CAtom::eElement_h &&
                (code==" H  " || code==" HA " || code=="1HA " || code=="2HA " ||
                 code==" HXT" || code=="1H  " || code=="2H  " || code=="3H  "))
            )
            return Residue::eCompleteBackboneAtom;

        // anything else is side chain
        return Residue::eSideChainAtom;

    } else if (residue->IsNucleotide()) {

        // nucleic acid Phosphorus
        if (element==CAtom::eElement_p && code==" P  ")
            return Residue::eAlphaBackboneAtom;

        // nucleic acid partial backbone
        if (
            (element==CAtom::eElement_c && (code==" C5*" || code==" C4*" || code==" C3*")) ||
            (element==CAtom::eElement_o && (code==" O5*" || code==" O3*")) ||
            (element==CAtom::eElement_h && (code==" H3T" || code==" H5T"))
           )
            return Residue::ePartialBackboneAtom;

        // nucleic acid complete backbone (all backbone that's not part of "partial")
        if (
            (element==CAtom::eElement_o &&
                (code==" O1P" || code==" O2P" || code==" O4*" || code==" O2*")) ||
            (element==CAtom::eElement_c && (code==" C2*" || code==" C1*")) ||
            (element==CAtom::eElement_h &&
                (code=="1H5*" || code=="2H5*" || code==" H4*" || code==" H3*" ||
                 code==" H2*" || code==" H1*" || code==" H1P" || code==" H2P" ||
                 code==" HO2" || code=="1H2*" || code=="2H2*"))
           )
            return Residue::eCompleteBackboneAtom;

        // anything else is side chain
        return Residue::eSideChainAtom;
    }

    return Residue::eUnknownAtom;
}

Residue::Residue(StructureBase *parent,
    const CResidue& residue, int moleculeID,
    const ResidueGraphList& standardDictionary,
    const ResidueGraphList& localDictionary) :
    StructureBase(parent), code(NO_CODE), alphaID(NO_ALPHA_ID), type(eOther)
{
    // get ID
    id = residue.GetId().Get();

    // get Residue name
    if (residue.IsSetName()) namePDB = residue.GetName();

    // get CResidue_graph*
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
        if (i->GetObject().GetId().Get() == graphID) {
            residueGraph = i->GetPointer();
            break;
        }
    }
    if (!residueGraph)
        ERR_POST(Fatal << "confused by Molecule #?, Residue #" << id
            << "; can't find Residue-graph ID #" << graphID);

    // get iupac-code if present - assume it's the first character of the first VisibleString
    if (residueGraph->IsSetIupac_code())
        code = residueGraph->GetIupac_code().front()[0];

    // get residue-graph name if present
    if (residueGraph->IsSetDescr()) {
        CResidue_graph::TDescr::const_iterator j, je = residueGraph->GetDescr().end();
        for (j=residueGraph->GetDescr().begin(); j!=je; j++) {
            if (j->GetObject().IsName()) {
                nameGraph = j->GetObject().GetName();
                break;
            }
        }
    }

    // get type
    if (residueGraph->IsSetResidue_type())
        type = static_cast<eType>(residueGraph->GetResidue_type());

    // get StructureObject* parent
    const StructureObject *object;
    if (!GetParentOfType(&object) || object->coordSets.size() == 0) {
        ERR_POST("Residue() : parent doesn't have any CoordSets");
        return;
    }

    // get atom info
    nAtomsPresentInAllCoordSets = 0;
    CResidue_graph::TAtoms::const_iterator a, ae = residueGraph->GetAtoms().end();
    for (a=residueGraph->GetAtoms().begin(); a!=ae; a++) {

        const CAtom& atom = a->GetObject();
        int atomID = atom.GetId().Get();
        AtomInfo *info = new AtomInfo;
        AtomPntr ap(moleculeID, id, atomID);

        // first see if this atom is present each CoordSet; if not, don't
        // bother storing info. This forces an intersection on CoordSets - e.g.,
        // from a multi-model NMR structure, only those atoms present in *all*
        // models will be displayed.
        StructureObject::CoordSetList::const_iterator c, ce=object->coordSets.end();
        for (c=object->coordSets.begin(); c!=ce; c++) {
            if (!((*c)->atomSet->GetAtom(ap, true, true))) break;
        }
        info->isPresentInAllCoordSets = (c == ce);

        info->residue = this;
        // get name if present
        if (atom.IsSetName()) info->name = atom.GetName();
        // get code if present - just use first one of the SEQUENCE
        if (atom.IsSetIupac_code())
            info->code = atom.GetIupac_code().front();
        // get atomic number, assuming it's the integer value of the enumerated type
	CAtom_Base::EElement atomicNumber = atom.GetElement();
        info->atomicNumber = static_cast<int>(atomicNumber);
        // get ionizable status
        if (atom.IsSetIonizable_proton() &&
            atom.GetIonizable_proton() == CAtom::eIonizable_proton_true)
            info->isIonizableProton = true;
        else
            info->isIonizableProton = false;
        // assign (unique) name
        info->glName = parentSet->CreateName(this, atomID);

        // classify atom
        info->classification = ClassifyAtom(this, atom);
        // store alphaID in residue
        if (info->classification == eAlphaBackboneAtom) alphaID = atomID;

        // add info to map
        if (atomInfos.find(atom.GetId().Get()) != atomInfos.end())
            ERR_POST(Error << "Residue #" << id << ": confused by multiple atom IDs " << atom.GetId().Get());
        atomInfos[atomID] = info;
        nAtomsPresentInAllCoordSets++;
    }

    // get bonds
    CResidue_graph::TBonds::const_iterator b, be = residueGraph->GetBonds().end();
    for (b=residueGraph->GetBonds().begin(); b!=be; b++) {
        int order = b->GetObject().IsSetBond_order() ?
                b->GetObject().GetBond_order() : Bond::eUnknown;
        const Bond *bond = MakeBond(this,
            moleculeID, id, b->GetObject().GetAtom_id_1().Get(),
            moleculeID, id, b->GetObject().GetAtom_id_2().Get(),
            order);
        if (bond) bonds.push_back(bond);
    }
}

Residue::~Residue(void)
{
    AtomInfoMap::iterator a, ae = atomInfos.end();
    for (a=atomInfos.begin(); a!=ae; a++) delete a->second;
}

// draw atom spheres here
bool Residue::Draw(const AtomSet *atomSet) const
{
    if (!atomSet) {
        ERR_POST(Error << "Residue::Draw(data) - NULL AtomSet*");
        return false;
    }

    // verify presense of OpenGLRenderer
    if (!parentSet->renderer) {
        ERR_POST(Warning << "Residue::Draw() - no renderer");
        return false;
    }

    // get Molecule parent
    const Molecule *molecule;
    if (!GetParentOfType(&molecule)) return false;

    // get object parent
    const StructureObject *object;
    if (!GetParentOfType(&object)) return false;

    bool overlayEnsembles = parentSet->showHideManager->OverlayConfEnsembles();
    AtomStyle atomStyle;
    const AtomCoord *atom;
    double alpha;

    // iterate atoms; key is atomID
    AtomInfoMap::const_iterator a, ae = atomInfos.end();
    for (a=atomInfos.begin(); a!=ae; a++) {

        if (!a->second->isPresentInAllCoordSets) continue;
        AtomPntr ap(molecule->id, id, a->first);

        // get Style
        if (!parentSet->styleManager->GetAtomStyle(this, ap, &atomStyle))
            return false;

        // use highlighting color if necessary
        if (atomStyle.isHighlighted)
            atomStyle.color = GlobalColors()->Get(Colors::eHighlight);

        // get AtomCoord* for appropriate altConf and draw the atom
        if (atomStyle.style != StyleManager::eNotDisplayed &&
            atomStyle.radius > 0.0 &&
            (atom = atomSet->GetAtom(ap, overlayEnsembles)) != NULL) {

            // add transparency; scale by occupancy if transparent
            if (atomStyle.style == StyleManager::eTransparentAtom) {
                alpha = 0.6;
                if (atom->occupancy < 1 && atom->occupancy > 0)
                    alpha *= atom->occupancy;
            } else
                alpha = 1.0;

            parentSet->renderer->DrawAtom(atom->site, atomStyle, alpha);
        }
    }

    return true;
}

END_SCOPE(Cn3D)

