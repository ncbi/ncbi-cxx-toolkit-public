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

#ifndef CN3D_MOLECULE__HPP
#define CN3D_MOLECULE__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <string>
#include <vector>

#include <objects/mmdb1/Molecule_graph.hpp>
#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Biomol_descr.hpp>

#include "structure_base.hpp"
#include "residue.hpp"
#include "vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// A Molecule is generally a fully connected set of atoms - e.g. a protein chain,
// heterogen, etc. For proteins and nucleotides, it is divided into an ordered
// sequence of Residues, along with inter-residue bonds.

typedef std::list< ncbi::CRef< ncbi::objects::CResidue_graph > > ResidueGraphList;

class ChemicalGraph;
class Bond;
class Sequence;
class MoleculeIdentifier;

class Molecule : public StructureBase
{
public:
    Molecule(ChemicalGraph *parentGraph,
        const ncbi::objects::CMolecule_graph& graph,
        const ResidueGraphList& standardDictionary,
        const ResidueGraphList& localDictionary);

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
    int id;
    std::string name;
    const MoleculeIdentifier *identifier;

    typedef std::map < int, const Residue * > ResidueMap;
    ResidueMap residues;
    typedef std::list < const Bond * > BondList;
    BondList interResidueBonds; // includes virtual and disulfide bonds

    // ints are residue IDs; tracks intramolecular disulfides (mainly for fast lookup by threader)
    typedef std::map < int, int > DisulfideMap;
    DisulfideMap disulfideMap;

    // maps of sequence location ( = residueID - 1) to secondary structure and domains
    static const int NO_DOMAIN_SET;
    enum eSecStruc {
        eHelix,
        eStrand,
        eCoil
    };
    std::vector < eSecStruc > residueSecondaryStructures;
    std::vector < int > residueDomains;
    int nDomains;

    // corresponding sequence (if present)
    const Sequence *sequence;

    typedef std::list < unsigned int > DisplayListList;
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

    // get coordinates for alpha atoms of residues with given sequence indexes;
    // returns actual # coordinates retrieved if successful, -1 on failure
    int GetAlphaCoords(int nResidues, const int *seqIndexes, const Vector * *coords) const;

    // secondary structure query methods

    bool IsResidueInHelix(int residueID) const
        { return (IsProtein() && residueSecondaryStructures[residueID - 1] == eHelix); }
    bool IsResidueInStrand(int residueID) const
        { return (IsProtein() && residueSecondaryStructures[residueID - 1] == eStrand); }
    bool IsResidueInCoil(int residueID) const
        { return (!IsProtein() || residueSecondaryStructures[residueID - 1] == eCoil); }

    // domain query
    int ResidueDomainID(int residueID) const
        { return residueDomains[residueID - 1]; }

    // drawing - include chain termini labels
    bool DrawAllWithTerminiLabels(const AtomSet *atomSet = NULL) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_MOLECULE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2004/05/20 18:49:21  thiessen
* don't do structure realignment if < 3 coords present
*
* Revision 1.29  2004/02/19 17:04:58  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.28  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.27  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.26  2001/08/21 01:10:13  thiessen
* add labeling
*
* Revision 1.25  2001/08/15 20:52:58  juran
* Heed warnings.
*
* Revision 1.24  2001/06/21 02:01:07  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.23  2001/05/31 18:46:26  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.22  2001/05/15 23:49:20  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.21  2001/03/23 23:31:30  thiessen
* keep atom info around even if coords not all present; mainly for disulfide parsing in virtual models
*
* Revision 1.20  2001/03/23 04:18:20  thiessen
* parse and display disulfides
*
* Revision 1.19  2001/02/08 23:01:13  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.18  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.17  2000/12/01 19:34:43  thiessen
* better domain assignment
*
* Revision 1.16  2000/11/30 15:49:08  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.15  2000/11/13 18:05:58  thiessen
* working structure re-superpositioning
*
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
*/
