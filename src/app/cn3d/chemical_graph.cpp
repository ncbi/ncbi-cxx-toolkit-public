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
* Revision 1.14  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.13  2000/09/11 01:46:14  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.12  2000/08/27 18:52:20  thiessen
* extract sequence information
*
* Revision 1.11  2000/08/21 17:22:37  thiessen
* add primitive highlighting for testing
*
* Revision 1.10  2000/08/17 14:24:05  thiessen
* added working StyleManager
*
* Revision 1.9  2000/08/16 14:18:44  thiessen
* map 3-d objects to molecules
*
* Revision 1.8  2000/08/13 02:43:00  thiessen
* added helix and strand objects
*
* Revision 1.7  2000/08/07 14:13:15  thiessen
* added animation frames
*
* Revision 1.6  2000/08/07 00:21:17  thiessen
* add display list mechanism
*
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
#include "cn3d/structure_set.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/atom_set.hpp"
#include "cn3d/object_3d.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

static const CBiostruc_residue_graph_set* standardDictionary = NULL;

void LoadStandardDictionary(void)
{
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

void DeleteStandardDictionary(void)
{
    if (standardDictionary) delete standardDictionary;
}


ChemicalGraph::ChemicalGraph(StructureBase *parent, const CBiostruc_graph& graph) :
    StructureBase(parent), displayListOtherStart(OpenGLRenderer::NO_LIST)
{
    if (!standardDictionary) {
        ERR_POST(Fatal << "need to load standard dictionary first");
        return;
    }

    // figure out what models we'll be drawing, based on contents of parent 
    // StructureSet and StructureObject
    const StructureObject *object;
    if (!GetParentOfType(&object)) return;

    // if this is the only StructureObject in this StructureSet, and if this
    // StructureObject has only one CoordSet, and if this CoordSet's AtomSet
    // has multiple ensembles, then use multiple altConf ensemble
    int nAlts = 0;
    if (!parentSet->isMultipleStructure && object->coordSets.size() == 1 &&
        object->coordSets.front()->atomSet->ensembles.size() > 1) {

        AtomSet *atomSet = object->coordSets.front()->atomSet;
        AtomSet::EnsembleList::iterator e, ee=atomSet->ensembles.end();
        for (e=atomSet->ensembles.begin(); e!=ee; e++) {
            atomSetList.push_back(std::make_pair(atomSet, *e));
            nAlts++;
        }

    // otherwise, use all CoordSets using default altConf ensemble for single
    // structure; for multiple structure StructureSet, use only first CoordSet
    } else {
        StructureObject::CoordSetList::const_iterator c, ce=object->coordSets.end();
        for (c=object->coordSets.begin(); c!=ce; c++) {
            atomSetList.push_back(std::make_pair((*c)->atomSet,
                reinterpret_cast<const std::string *>(NULL)));   // VC++ requires this cast for some reason...
            nAlts++;
            if (parentSet->isMultipleStructure) break;
        }
    }
    TESTMSG("nAlts = " << nAlts);
    if (!nAlts) {
        ERR_POST(Warning << "ChemicalGraph has zero AtomSets!");
        return;
    }

    unsigned int firstNewFrame = parentSet->frameMap.size();
    parentSet->frameMap.resize(firstNewFrame + nAlts);

    // load molecules from SEQUENCE OF Molecule-graph
    CBiostruc_graph::TMolecule_graphs::const_iterator i, ie=graph.GetMolecule_graphs().end();
    for (i=graph.GetMolecule_graphs().begin(); i!=ie; i++) {
        Molecule *molecule = new Molecule(this,
            i->GetObject(),
            standardDictionary->GetResidue_graphs(),
            graph.GetResidue_graphs());

        if (molecules.find(molecule->id) != molecules.end())
            ERR_POST(Error << "confused by repeated Molecule-graph ID's");
        molecules[molecule->id] = molecule;

        // set molecules' display list(s); each protein or nucleotide molecule
        // gets its own display list(s) (one display list for each molecule for
        // each set of coordinates), while everything else - hets, solvents,
        // inter-molecule bonds - goes in a single list(s).
        for (unsigned int n=0; n<nAlts; n++) {

            if (molecule->IsProtein() || molecule->IsNucleotide()) {
                molecule->displayLists.push_back(++(parentSet->lastDisplayList));
                // add molecule's display list to frame
                parentSet->frameMap[firstNewFrame + n].push_back(parentSet->lastDisplayList);

            } else { // het/solvent
                if (displayListOtherStart == OpenGLRenderer::NO_LIST) {
                    displayListOtherStart = parentSet->lastDisplayList + 1;
                    parentSet->lastDisplayList += nAlts;
                }
                molecule->displayLists.push_back(displayListOtherStart + n);
            }
        }
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

            if (!bond) continue; // can happen bond if bond is to atom not present

            interMoleculeBonds.push_back(bond);

            // set inter-molecule bonds' display list(s)
            if (displayListOtherStart == OpenGLRenderer::NO_LIST) {
                displayListOtherStart = parentSet->lastDisplayList + 1;
                parentSet->lastDisplayList += nAlts;
            }
        }
    }

    // if hets/solvent/i-m bonds present, add display lists to frames
    if (displayListOtherStart != OpenGLRenderer::NO_LIST) {
        for (unsigned int n=0; n<nAlts; n++)
            parentSet->frameMap[firstNewFrame + n].push_back(displayListOtherStart + n);
    }
}

static int moleculeToRedraw = -1;

void ChemicalGraph::RedrawMolecule(int moleculeID) const
{
    moleculeToRedraw = moleculeID;
    DrawAll(NULL);
    moleculeToRedraw = -1;
}

// This is where the work of breaking objects up into display lists gets done.
bool ChemicalGraph::DrawAll(const AtomSet *ignored) const
{
    const StructureObject *object;
    if (!GetParentOfType(&object)) return false;

    if (moleculeToRedraw != -1)
        TESTMSG("drawing molecule " << moleculeToRedraw
            << " of ChemicalGraph of object " << object->pdbID);
    else
        TESTMSG("drawing ChemicalGraph of object " << object->pdbID);

    // put each protein (with its 3d-objects) or nucleotide chain in its own display list
    bool continueDraw;
    AtomSetList::const_iterator a, ae=atomSetList.end();
    MoleculeMap::const_iterator m, me=molecules.end();
    for (m=molecules.begin(); m!=me; m++) {
        if (!m->second->IsProtein() && !m->second->IsNucleotide()) continue;
        if (moleculeToRedraw >= 0 && m->second->id != moleculeToRedraw) continue;

        Molecule::DisplayListList::const_iterator md=m->second->displayLists.begin();
        for (a=atomSetList.begin(); a!=ae; a++, md++) {

            // start new display list
            //TESTMSG("drawing molecule #" << m->second->id << " + its objects in display list " << *md
            //        << " of " << atomSetList.size());
            parentSet->renderer->StartDisplayList(*md);

            // apply relative transformation if this is a slave structure
            if (object->IsSlave()) parentSet->renderer->PushMatrix(object->transformToMaster);

            // draw this molecule with all alternative AtomSets (e.g., NMR's or altConfs)
            a->first->SetActiveEnsemble(a->second);
            continueDraw = m->second->DrawAll(a->first);

            if (continueDraw) {
                // find objects for this molecule;
                // only use 3d-objects from first coordset, since they will only be
                // present in a model with a single set of coordinates, anyway
                CoordSet::Object3DMap::const_iterator
                    objList = object->coordSets.front()->objectMap.find(m->second->id);
                if (objList != object->coordSets.front()->objectMap.end()) {
                    CoordSet::Object3DList::const_iterator o, oe=objList->second.end();
                    for (o=objList->second.begin(); o!=oe; o++) {
                        if (!(continueDraw = (*o)->Draw(a->first))) break;
                    }
                }
            }

            // revert transformation matrix
            if (object->IsSlave()) parentSet->renderer->PopMatrix();

            // end display list
            parentSet->renderer->EndDisplayList();

            if (!continueDraw) return false;
        }

        // we're done if this was the single molecule meant to be redrawn
        if (moleculeToRedraw >= 0) break;
    }

    // then put everything else (solvents, hets, intermolecule bonds) in a single display list
    if (displayListOtherStart == OpenGLRenderer::NO_LIST) return true;
    //TESTMSG("drawing hets/solvents/i-m bonds");
    
    // always redraw all these even if only a single molecule is to be redrawn -
    // that way connections can show/hide in cases where a particular residue
    // changes its display
    int n = 0;
    for (a=atomSetList.begin(); a!=ae; a++, n++) {
    
        a->first->SetActiveEnsemble(a->second);
        parentSet->renderer->StartDisplayList(displayListOtherStart + n);
        if (object->IsSlave()) parentSet->renderer->PushMatrix(object->transformToMaster);

        for (m=molecules.begin(); m!=me; m++) {
            if (m->second->IsProtein() || m->second->IsNucleotide()) continue;
            if (!(continueDraw = m->second->DrawAll(a->first))) break;
        }

        if (continueDraw) {
            BondList::const_iterator b, be=interMoleculeBonds.end();
            for (b=interMoleculeBonds.begin(); b!=be; b++) {
                if (!(continueDraw = (*b)->Draw(a->first))) break;
            }
        }

        if (object->IsSlave()) parentSet->renderer->PopMatrix();
        parentSet->renderer->EndDisplayList();

        if (!continueDraw) return false;
    }

    return true;
}

END_SCOPE(Cn3D)

