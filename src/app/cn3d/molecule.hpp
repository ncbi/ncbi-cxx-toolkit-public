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

#include <map>

#include <objects/mmdb1/Molecule_graph.hpp>
#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Biomol_descr.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/residue.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

// A Molecule is generally a fully connected set of atoms - e.g. a protein chain,
// heterogen, etc. For proteins and nucleotides, it is divided into an ordered
// sequence of Residues, along with inter-residue bonds.

typedef list< CRef< CResidue_graph > > ResidueGraphList;

class Bond;

class Molecule : public StructureBase
{
public:
    Molecule(StructureBase *parent,
        const CMolecule_graph& graph,
        const ResidueGraphList& standardDictionary,
        const ResidueGraphList& localDictionary);
    //~Molecule(void);

    // public data
    enum eType {
        eDNA = CBiomol_descr::eMolecule_type_dna,
        eRNA = CBiomol_descr::eMolecule_type_rna,
        eProtein = CBiomol_descr::eMolecule_type_protein,
        eBiopolymer = CBiomol_descr::eMolecule_type_other_biopolymer,
        eSolvent = CBiomol_descr::eMolecule_type_solvent,
        eNonpolymer = CBiomol_descr::eMolecule_type_other_nonpolymer,
        eOther = CBiomol_descr::eMolecule_type_other
    };
    eType type;
    int id;

    typedef std::map < int, const Residue * > ResidueMap;
    ResidueMap residues;
    typedef LIST_TYPE < const Bond * > BondList;
    BondList interResidueBonds;

    // public methods
    bool IsProtein(void) { return (type == eProtein); }
    bool IsNucleotide(void) { return (type == eDNA || type == eRNA); }
    bool IsSolvent(void) { return (type == eSolvent); }
    bool IsHeterogen(void) { return (!IsProtein() && !IsNucleotide() && !IsSolvent()); }
    const Residue::AtomInfo * GetAtomInfo(int rID, int aID) const
    { 
        ResidueMap::const_iterator info=residues.find(rID);
        if (info != residues.end()) return (*info).second->GetAtomInfo(aID);
        ERR_POST(Warning << "Molecule #" << id << ": can't find residue #" << rID);
        return NULL;
    }
    //bool Draw(const StructureBase *data) const;

private:
};

END_SCOPE(Cn3D)

#endif // CN3D_MOLECULE__HPP
