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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2000/08/21 17:22:45  thiessen
* add primitive highlighting for testing
*
* Revision 1.7  2000/08/07 00:20:18  thiessen
* add display list mechanism
*
* Revision 1.6  2000/08/03 15:12:29  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.5  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.4  2000/07/16 23:18:33  thiessen
* redo of drawing system
*
* Revision 1.3  2000/07/12 23:28:27  thiessen
* now draws basic CPK model
*
* Revision 1.2  2000/07/12 02:00:39  thiessen
* add basic wxWindows GUI
*
* Revision 1.1  2000/07/11 13:49:27  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#ifndef CN3D_CHEMICALGRAPH__HPP
#define CN3D_CHEMICALGRAPH__HPP

#include <map>
#include <string>

#include <objects/mmdb1/Biostruc_graph.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/residue.hpp"

BEGIN_SCOPE(Cn3D)

// The ChemicalGraph is the set of bonds that link the atoms (from CoordSets).
// The graph is divided into essentially physically separate molecules (e.g.,
// protein chains, hets, solvents), occasionally joined by inter-molecule
// bonds ("connections" in Cn3D jargon).

class Bond;
class AtomSet;
class StructureObject;

class ChemicalGraph : public StructureBase
{
public:
    ChemicalGraph(StructureBase *parent, const ncbi::objects::CBiostruc_graph& graph);
    //~ChemicalGraph(void);

    // public data
    typedef std::map < int, const Molecule * > MoleculeMap;
    MoleculeMap molecules;
    typedef LIST_TYPE < const Bond * > BondList;
    BondList interMoleculeBonds;

    // public methods

    void RedrawMolecule(int moleculeID);
    bool DrawAll(const AtomSet *atomSet = NULL) const;
    const Residue::AtomInfo * GetAtomInfo(const AtomPntr& atom) const
    { 
        MoleculeMap::const_iterator info=molecules.find(atom.mID);
        if (info != molecules.end()) return (*info).second->GetAtomInfo(atom.rID, atom.aID);
        ERR_POST(ncbi::Warning << "Graph: can't find molecule #" << atom.mID);
        return NULL;
    }

private:
    typedef LIST_TYPE < std::pair < AtomSet *, const std::string * > > AtomSetList;
    AtomSetList atomSetList;
    unsigned int displayListOtherStart;
    int moleculeToRedraw;
};

END_SCOPE(Cn3D)

#endif // CN3D_CHEMICALGRAPH__HPP
