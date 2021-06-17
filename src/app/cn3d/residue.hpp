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
* ===========================================================================
*/

#ifndef CN3D_RESIDUE__HPP
#define CN3D_RESIDUE__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <string>

#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Residue.hpp>

#include "structure_base.hpp"

BEGIN_SCOPE(Cn3D)

typedef std::list< ncbi::CRef< ncbi::objects::CResidue_graph > > ResidueGraphList;

// a Residue is a set of bonds that connect one residue of a larger Molecule.
// Its constructor is where most of the work of decoding the ASN1 graph is done,
// based on the standard and local residue dictionaries. Each Residue also holds
// information (AtomInfo) about the nature of the atoms it contains.

class Bond;

class Residue : public StructureBase
{
public:
    Residue(StructureBase *parent,
        const ncbi::objects::CResidue& residue, int moleculeID,
        const ResidueGraphList& standardDictionary,
        const ResidueGraphList& localDictionary,
        int nResidues, int moleculeType);
    ~Residue(void);

    // public data
    int id;
    static const char NO_CODE;
    char code;
    std::string
        nameGraph,  // 'name' field from residue-graph dictionary
        namePDB;    // 'name' in Residue, supposed to correspond to PDB-assigned residue number
    static const int NO_ALPHA_ID;
    int alphaID; // ID of "alpha" atom (C-alpha or P)

    // residue type
    enum eType {
        eDNA = ncbi::objects::CResidue_graph::eResidue_type_deoxyribonucleotide,
        eRNA = ncbi::objects::CResidue_graph::eResidue_type_ribonucleotide,
        eAminoAcid = ncbi::objects::CResidue_graph::eResidue_type_amino_acid,
        eOther = ncbi::objects::CResidue_graph::eResidue_type_other
    };
    eType type;

    // atom type
    enum eAtomClassification {
        eSideChainAtom,
        eAlphaBackboneAtom,     // C-alpha or P
        ePartialBackboneAtom,   // for unbranched backbone trace
        eCompleteBackboneAtom,  // all backbone atoms
        eUnknownAtom            // anything that's not known to be of an amino acid or nucleotide
    };

    typedef struct {
        std::string name, code;
        int atomicNumber;
        eAtomClassification classification;
        unsigned int glName;
        const Residue *residue;  // convenient way to go from atom->residue
        bool isIonizableProton;
    } AtomInfo;

    typedef std::list < const Bond * > BondList;
    BondList bonds;

    // public methods
    bool HasCode(void) const { return (code != NO_CODE); }
    bool HasName(void) const { return (!nameGraph.empty()); }
    bool IsNucleotide(void) const { return (type == eDNA || type == eRNA); }
    bool IsAminoAcid(void) const { return (type == eAminoAcid); }
    bool Draw(const AtomSet *atomSet) const;

    typedef std::map < int , const AtomInfo * > AtomInfoMap;

private:
    AtomInfoMap atomInfos;  // mapped by Atom-id
    int nAtomsWithAnyCoords;

public:
    // # atoms in the graph for this residue
    int NAtomsInGraph(void) const { return atomInfos.size(); }
    // # atoms in this residue with real coordinates in any model
    int NAtomsWithAnyCoords(void) const { return nAtomsWithAnyCoords; }

    const AtomInfoMap& GetAtomInfos(void) const { return atomInfos; }
    const AtomInfo * GetAtomInfo(int aID) const
    {
        AtomInfoMap::const_iterator info=atomInfos.find(aID);
        if (info != atomInfos.end()) return (*info).second;
        ERR_POST(ncbi::Warning << "Residue #" << id << ": can't find atom #" << aID);
        return NULL;
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_RESIDUE__HPP
