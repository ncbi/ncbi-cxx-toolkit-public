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
* Revision 1.14  2000/10/04 17:40:46  thiessen
* rearrange STL #includes
*
* Revision 1.13  2000/09/08 20:16:10  thiessen
* working dynamic alignment views
*
* Revision 1.12  2000/09/03 18:45:56  thiessen
* working generalized sequence viewer
*
* Revision 1.11  2000/08/30 19:49:03  thiessen
* working sequence window
*
* Revision 1.10  2000/08/28 23:46:46  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.9  2000/08/28 18:52:18  thiessen
* start unpacking alignments
*
* Revision 1.8  2000/08/27 18:50:55  thiessen
* extract sequence information
*
* Revision 1.7  2000/08/24 18:43:15  thiessen
* tweaks for transparent sphere display
*
* Revision 1.6  2000/08/07 00:20:18  thiessen
* add display list mechanism
*
* Revision 1.5  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.4  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.3  2000/07/17 04:21:09  thiessen
* now does correct structure alignment transformation
*
* Revision 1.2  2000/07/16 23:18:34  thiessen
* redo of drawing system
*
* Revision 1.1  2000/07/11 13:49:29  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#ifndef CN3D_MOLECULE__HPP
#define CN3D_MOLECULE__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <string>

#include <objects/mmdb1/Molecule_graph.hpp>
#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Biomol_descr.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/residue.hpp"
#include "cn3d/vector_math.hpp"

BEGIN_SCOPE(Cn3D)

// A Molecule is generally a fully connected set of atoms - e.g. a protein chain,
// heterogen, etc. For proteins and nucleotides, it is divided into an ordered
// sequence of Residues, along with inter-residue bonds.

typedef list< ncbi::CRef< ncbi::objects::CResidue_graph > > ResidueGraphList;

class Bond;
class Sequence;

class Molecule : public StructureBase
{
public:
    Molecule(StructureBase *parent,
        const ncbi::objects::CMolecule_graph& graph,
        const ResidueGraphList& standardDictionary,
        const ResidueGraphList& localDictionary);
    //~Molecule(void);

    // public data
    enum eType {
        eDNA = ncbi::objects::CBiomol_descr::eMolecule_type_dna,
        eRNA = ncbi::objects::CBiomol_descr::eMolecule_type_rna,
        eProtein = ncbi::objects::CBiomol_descr::eMolecule_type_protein,
        eBiopolymer = ncbi::objects::CBiomol_descr::eMolecule_type_other_biopolymer,
        eSolvent = ncbi::objects::CBiomol_descr::eMolecule_type_solvent,
        eNonpolymer = ncbi::objects::CBiomol_descr::eMolecule_type_other_nonpolymer,
        eOther = ncbi::objects::CBiomol_descr::eMolecule_type_other
    };
    eType type;
    static const int NOT_SET;
    int id, gi, pdbChain;
    std::string name, pdbID;

    typedef std::map < int, const Residue * > ResidueMap;
    ResidueMap residues;
    typedef LIST_TYPE < const Bond * > BondList;
    BondList interResidueBonds;
    BondList virtualBonds;

    // corresponding sequence (if present)
    const Sequence *sequence;

    typedef LIST_TYPE < unsigned int > DisplayListList;
    DisplayListList displayLists;

    // public methods
    bool IsProtein(void) const { return (type == eProtein); }
    bool IsNucleotide(void) const { return (type == eDNA || type == eRNA); }
    bool IsSolvent(void) const { return (type == eSolvent); }
    bool IsHeterogen(void) const { return (!IsProtein() && !IsNucleotide() && !IsSolvent()); }

    int NResidues(void) const { return residues.size(); }
    const Residue::AtomInfo * GetAtomInfo(int rID, int aID) const
    { 
        ResidueMap::const_iterator info=residues.find(rID);
        if (info != residues.end()) return (*info).second->GetAtomInfo(aID);
        ERR_POST(ncbi::Warning << "Molecule #" << id << ": can't find residue #" << rID);
        return NULL;
    }

    // residue color method - called by sequence/alignment viewer - note
    // that sequenceIndex is numbered from zero.
    Vector GetResidueColor(int sequenceIndex) const;

private:
};

END_SCOPE(Cn3D)

#endif // CN3D_MOLECULE__HPP
