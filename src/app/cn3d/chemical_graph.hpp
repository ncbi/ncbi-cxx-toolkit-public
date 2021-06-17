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
*      Classes to hold the graph of chemical bonds
*
* ===========================================================================
*/

#ifndef CN3D_CHEMICALGRAPH__HPP
#define CN3D_CHEMICALGRAPH__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <string>

#include <objects/mmdb1/Biostruc_graph.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb1/Atom_pntr.hpp>

#include "structure_base.hpp"
#include "molecule.hpp"
#include "residue.hpp"

BEGIN_SCOPE(Cn3D)

// to create and delete dictionary - should be called on program init/exit
void LoadStandardDictionary(const char *filename);
void DeleteStandardDictionary(void);


// The ChemicalGraph is the set of bonds that link the atoms (from CoordSets).
// The graph is divided into essentially physically separate molecules (e.g.,
// protein chains, hets, solvents), occasionally joined by inter-molecule
// bonds ("connections" in Cn3D jargon).

class Bond;
class AtomSet;
class StructureObject;
class Molecule;

typedef std::list< ncbi::CRef< ncbi::objects::CBiostruc_feature_set > > FeatureList;

class ChemicalGraph : public StructureBase
{
public:
    ChemicalGraph(StructureBase *parent, const ncbi::objects::CBiostruc_graph& graph,
        const FeatureList& features);

    // public data
    std::string name;
    typedef std::map < int, const Molecule * > MoleculeMap;
    MoleculeMap molecules;
    typedef std::list < const Bond * > BondList;
    BondList interMoleculeBonds;    // includes inter-molecular disulfides

    // public methods

    void RedrawMolecule(int moleculeID) const;
    bool DrawAll(const AtomSet *atomSet = NULL) const;
    const Residue::AtomInfo * GetAtomInfo(const AtomPntr& atom) const
    {
        MoleculeMap::const_iterator info=molecules.find(atom.mID);
        if (info != molecules.end()) return (*info).second->GetAtomInfo(atom.rID, atom.aID);
        ERR_POST(ncbi::Warning << "Graph: can't find molecule #" << atom.mID);
        return NULL;
    }

    // check if a bond between the given atoms is a disulfide bond; if so, flag Bond as disulfide, and
    // add new virtual disulfide Bond for these residues to the given bond list. Returns true if
    // a virtual disulfide was added to the bondList, false otherwise. (*Not* true or false depending
    // on whether the bond is actually a disulfide!)
    bool CheckForDisulfide(const Molecule *molecule,
        const ncbi::objects::CAtom_pntr& atomPtr1,
        const ncbi::objects::CAtom_pntr& atomPtr2,
        std::list < const Bond * > *bondList, Bond *bond, StructureBase *parent);

private:
    typedef std::list < std::pair < AtomSet *, const std::string * > > AtomSetList;
    AtomSetList atomSetList;
    unsigned int displayListOtherStart;

    void UnpackDomainFeatures(const ncbi::objects::CBiostruc_feature_set& featureSet);
    void UnpackSecondaryStructureFeatures(const ncbi::objects::CBiostruc_feature_set& featureSet);
};

END_SCOPE(Cn3D)

#endif // CN3D_CHEMICALGRAPH__HPP
