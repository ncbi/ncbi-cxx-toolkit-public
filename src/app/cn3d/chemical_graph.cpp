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
* Revision 1.5  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.4  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.3  2000/07/16 23:19:10  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/12 23:27:49  thiessen
* now draws basic CPK model
*
* Revision 1.1  2000/07/11 13:45:29  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#include <serial/serial.hpp>            
#include <serial/objistrasnb.hpp>
#include <objects/mmdb1/Biostruc_residue_graph_set.hpp>      
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/general/Dbtag.hpp>

#include "cn3d/chemical_graph.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/bond.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

ChemicalGraph::ChemicalGraph(StructureBase *parent, const CBiostruc_graph& graph) :
    StructureBase(parent)
{
    static const CBiostruc_residue_graph_set* standardDictionary = NULL;
    if (!standardDictionary) {
        standardDictionary = new CBiostruc_residue_graph_set;

        // initialize the binary input stream 
        auto_ptr<CNcbiIstream> inStream;
        inStream.reset(new CNcbiIfstream("bstdt.val", IOS_BASE::in | IOS_BASE::binary));
        if (!(*inStream))
            ERR_POST(Fatal << "Cannot open dictionary file 'bstdt.val'");

        // Associate ASN.1 binary serialization methods with the input 
        auto_ptr<CObjectIStream> inObject;
        inObject.reset(new CObjectIStreamAsnBinary(*inStream));

        // Read the dictionary data 
        *inObject >> *(const_cast<CBiostruc_residue_graph_set *>(standardDictionary));

        // make sure it's the right thing
        if (!standardDictionary->IsSetId() ||
            !standardDictionary->GetId().front().GetObject().IsOther_database() ||
            standardDictionary->GetId().front().GetObject().GetOther_database().GetDb() != "Standard residue dictionary" ||
            !standardDictionary->GetId().front().GetObject().GetOther_database().GetTag().IsId() ||
            standardDictionary->GetId().front().GetObject().GetOther_database().GetTag().GetId() != 1)
            ERR_POST(Fatal << "file 'bstdt.val' does not contain expected dictionary data");
    }

    // load molecules from SEQUENCE OF Molecule-graph
    CBiostruc_graph::TMolecule_graphs::const_iterator i, ie=graph.GetMolecule_graphs().end();
    for (i=graph.GetMolecule_graphs().begin(); i!=ie; i++) {
        Molecule *molecule = new Molecule(this,
            i->GetObject(),
            standardDictionary->GetResidue_graphs(),
            graph.GetResidue_graphs());
        if (molecules.find(molecule->id) != molecules.end())
            ERR_POST(Fatal << "confused by repeated Molecule-graph ID's");
        molecules[molecule->id] = molecule;
    }

    // load connections from SEQUENCE OF Inter-residue-bond OPTIONAL
    if (graph.IsSetInter_molecule_bonds()) {
        CBiostruc_graph::TInter_molecule_bonds::const_iterator j, je=graph.GetInter_molecule_bonds().end();
        for (j=graph.GetInter_molecule_bonds().begin(); j!=je; j++) {
            int order = j->GetObject().IsSetBond_order() ? 
                j->GetObject().GetBond_order() : Bond::eUnknown;
            const Bond *bond = MakeBond(this,
                j->GetObject().GetAtom_id_1(), 
                j->GetObject().GetAtom_id_2(),
                order);
            if (bond) interMoleculeBonds.push_back(bond);
        }
    }
}

END_SCOPE(Cn3D)

