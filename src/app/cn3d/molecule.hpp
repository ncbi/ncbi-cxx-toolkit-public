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
    bool IsBiopolymer(void) const { return (type == eProtein || type == eDNA || type == eRNA || type == eBiopolymer); }
    bool IsSolvent(void) const { return (type == eSolvent); }
    bool IsHeterogen(void) const { return (!IsProtein() && !IsNucleotide() && !IsSolvent()); }

    unsigned int NResidues(void) const { return residues.size(); }
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
