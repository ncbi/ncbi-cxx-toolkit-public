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
*      Classes to hold sets of structure data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2000/08/04 22:49:04  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.14  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.13  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.12  2000/07/18 16:50:11  thiessen
* more friendly rotation center setting
*
* Revision 1.11  2000/07/18 00:06:00  thiessen
* allow arbitrary rotation center
*
* Revision 1.10  2000/07/17 22:37:18  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.9  2000/07/17 04:20:50  thiessen
* now does correct structure alignment transformation
*
* Revision 1.8  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
* Revision 1.7  2000/07/12 02:00:15  thiessen
* add basic wxWindows GUI
*
* Revision 1.6  2000/07/11 13:45:31  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.5  2000/07/01 15:43:50  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.4  2000/06/29 16:46:07  thiessen
* use NCBI streams correctly
*
* Revision 1.3  2000/06/29 14:35:06  thiessen
* new atom_set files
*
* Revision 1.2  2000/06/28 13:07:55  thiessen
* store alt conf ensembles
*
* Revision 1.1  2000/06/27 20:09:40  thiessen
* initial checkin
*
* ===========================================================================
*/

#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/ncbimime/Biostruc_seqs.hpp>
#include <objects/ncbimime/Biostruc_align.hpp>
#include <objects/ncbimime/Entrez_general.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/mmdb2/Biostruc_model.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Chem_graph_alignment.hpp>
#include <objects/mmdb3/Transform.hpp>
#include <objects/mmdb3/Move.hpp>
#include <objects/mmdb3/Trans_matrix.hpp>
#include <objects/mmdb3/Rot_matrix.hpp>

#include "cn3d/structure_set.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/atom_set.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/show_hide_manager.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

StructureSet::StructureSet(const CNcbi_mime_asn1& mime) :
    StructureBase(NULL), renderer(NULL), lastAtomName(OpenGLRenderer::NO_NAME)
{
    StructureObject *object;
    parentSet = this;
    showHideManager = new ShowHideManager();
    styleManager = new StyleManager();
    if (!showHideManager || !styleManager) {
        ERR_POST(Fatal << "StructureSet::StructureSet() - out of memory");
        return;
    }
    
    // create StructureObjects from (list of) biostruc
    if (mime.IsStrucseq()) { 
        object = new StructureObject(this, mime.GetStrucseq().GetStructure(), true);
        objects.push_back(object);

    } else if (mime.IsStrucseqs()) {
        object = new StructureObject(this, mime.GetStrucseqs().GetStructure(), true);
        objects.push_back(object);

    } else if (mime.IsAlignstruc()) {
        TESTMSG("Master:");
        object = new StructureObject(this, mime.GetAlignstruc().GetMaster(), true);
        objects.push_back(object);
        const CBiostruc_align::TSlaves& slaves = mime.GetAlignstruc().GetSlaves();
		CBiostruc_align::TSlaves::const_iterator i, e=slaves.end();
        for (i=slaves.begin(); i!=e; i++) {
            TESTMSG("Slave:");
            object = new StructureObject(this, i->GetObject(), false);
            if (!object->SetTransformToMaster(
                    mime.GetAlignstruc().GetAlignments(),
                    objects.front()->mmdbID))
                ERR_POST(Error << "Can't get alignment for slave " << object->pdbID
                    << " with master " << objects.front()->pdbID);
            objects.push_back(object);
        }

    } else if (mime.IsEntrez() && mime.GetEntrez().GetData().IsStructure()) {
        object = new StructureObject(this, mime.GetEntrez().GetData().GetStructure(), true);
        objects.push_back(object);
    
    } else {
        ERR_POST(Fatal << "Can't (yet) handle that Ncbi-mime-asn1 type");
    }
}

StructureSet::~StructureSet(void)
{
    if (showHideManager) delete showHideManager;
    if (styleManager) delete styleManager;
}

void StructureSet::SetCenter(const Vector *given)
{
    Vector siteSum;
    int nAtoms = 0;
    double dist;
    maxDistFromCenter = 0.0;

    // set new center if given one
    if (given) center = *given;

    // loop trough all atoms twice - once to get average center, then once to
    // find max distance from this center
    for (int i=0; i<2; i++) {
        if (given && i==0) continue; // skip center calculation if given one
        ObjectList::const_iterator o, oe=objects.end();
        for (o=objects.begin(); o!=oe; o++) {
            StructureObject::CoordSetList::const_iterator c, ce=(*o)->coordSets.end();
            for (c=(*o)->coordSets.begin(); c!=ce; c++) {
                AtomSet::AtomMap::const_iterator a, ae=(*c)->atomSet->atomMap.end();
                for (a=(*c)->atomSet->atomMap.begin(); a!=ae; a++) {
                    Vector site(a->second.front()->site);
                    if ((*o)->IsSlave() && (*o)->transformToMaster)
                        ApplyTransformation(&site, *((*o)->transformToMaster));
                    if (i==0) {
                        siteSum += site;
                        nAtoms++;
                    } else {
                        dist = (site - center).length();
                        if (dist > maxDistFromCenter)
                            maxDistFromCenter = dist;
                    }
                }
            }
        }
        if (i==0) center = siteSum / nAtoms;
    }
    TESTMSG("center: " << center << ", maxDistFromCenter " << maxDistFromCenter);
    rotationCenter = center;

    // set camera to center on the selected point
    if (renderer) renderer->ResetCamera();
}

bool StructureSet::Draw(const AtomSet *atomSet) const
{
    TESTMSG("drawing StructureSet");
    return true;
}

unsigned int StructureSet::CreateName(const Residue *residue, int atomID)
{
    lastAtomName++;
    nameMap[lastAtomName] = std::make_pair(residue, atomID);
    return lastAtomName;
}

bool StructureSet::GetAtomFromName(unsigned int name, const Residue **residue, int *atomID)
{
    NameMap::const_iterator i = nameMap.find(name);
    if (i == nameMap.end()) return false;
    *residue = i->second.first;
    *atomID = i->second.second;
	return true;
}

void StructureSet::SelectedAtom(unsigned int name)
{
    const Residue *residue;
    int atomID;

    if (name == OpenGLRenderer::NO_NAME || !GetAtomFromName(name, &residue, &atomID)) {
        ERR_POST(Info << "nothing selected");
        return;
    }

    TESTMSG("residue " << residue->id << ", atom " << atomID);
}

const int StructureObject::NO_MMDB_ID = -1;

StructureObject::StructureObject(StructureBase *parent, const CBiostruc& biostruc, bool master) :
    StructureBase(parent), isMaster(master), mmdbID(NO_MMDB_ID), transformToMaster(NULL)
{
    // get MMDB id
    CBiostruc::TId::const_iterator j, je=biostruc.GetId().end();
    for (j=biostruc.GetId().begin(); j!=je; j++) {
        if (j->GetObject().IsMmdb_id()) {
            mmdbID = j->GetObject().GetMmdb_id().Get();
            break;
        }
    }
    TESTMSG("MMDB id " << mmdbID);

    // get PDB id
    if (biostruc.IsSetDescr()) {
        CBiostruc::TDescr::const_iterator k, ke=biostruc.GetDescr().end();
        for (k=biostruc.GetDescr().begin(); k!=ke; k++) {
            if (k->GetObject().IsName()) {
                pdbID = k->GetObject().GetName();
                break;
            }
        }
    }
    TESTMSG("PDB id " << pdbID);

    // get atom and feature spatial coordinates
    if (biostruc.IsSetModel()) {
        // iterate SEQUENCE OF Biostruc-model
        CBiostruc::TModel::const_iterator i, ie=biostruc.GetModel().end();
        for (i=biostruc.GetModel().begin(); i!=ie; i++) {
            if (i->GetObject().IsSetModel_coordinates()) {
                CoordSet *coordSet =
                    new CoordSet(this, i->GetObject().GetModel_coordinates());
                coordSets.push_back(coordSet);
            }
        }
    }

    // get graph - must be done after atom coordinates are loaded, so we can
    // avoid storing graph nodes for atoms not present in the model
    graph = new ChemicalGraph(this, biostruc.GetChemical_graph());
}

bool StructureObject::SetTransformToMaster(const CBiostruc_annot_set& annot, int masterMMDBID)
{
    CBiostruc_annot_set::TFeatures::const_iterator f1, f1e=annot.GetFeatures().end();
    for (f1=annot.GetFeatures().begin(); f1!=f1e; f1++) {
        CBiostruc_feature_set::TFeatures::const_iterator f2, f2e=f1->GetObject().GetFeatures().end();
        for (f2=f1->GetObject().GetFeatures().begin(); f2!=f2e; f2++) {
        
            // look for alignment feature
            if (f2->GetObject().IsSetType() &&
				f2->GetObject().GetType() == CBiostruc_feature::eType_alignment &&
                f2->GetObject().GetLocation().IsAlignment()) {
                const CChem_graph_alignment& graphAlign =
					f2->GetObject().GetLocation().GetAlignment();

                // find transform alignment of this object with master
                if (graphAlign.GetDimension() == 2 &&
                    graphAlign.GetBiostruc_ids().size() == 2 &&
                    graphAlign.IsSetTransform() &&
                    graphAlign.GetTransform().size() == 1 &&
                    graphAlign.GetBiostruc_ids().front().GetObject().IsMmdb_id() &&
                    graphAlign.GetBiostruc_ids().front().GetObject().GetMmdb_id().Get() == masterMMDBID &&
                    graphAlign.GetBiostruc_ids().back().GetObject().IsMmdb_id() &&
                    graphAlign.GetBiostruc_ids().back().GetObject().GetMmdb_id().Get() == mmdbID) {

                    TESTMSG("Got transform for " << pdbID << "->master");
                    // unpack transform into matrix, moves in reverse order;
                    Matrix xform;
                    transformToMaster = new Matrix();
                    CTransform::TMoves::const_iterator 
                        m, me=graphAlign.GetTransform().front().GetObject().GetMoves().end();
                    for (m=graphAlign.GetTransform().front().GetObject().GetMoves().begin(); m!=me; m++) {
                        Matrix xmat;
                        double scale;
                        if (m->GetObject().IsTranslate()) {
                            const CTrans_matrix& trans = m->GetObject().GetTranslate();
                            scale = 1.0 / trans.GetScale_factor();
                            SetTranslationMatrix(&xmat,
                                Vector(scale * trans.GetTran_1(),
                                       scale * trans.GetTran_2(),
                                       scale * trans.GetTran_3()));
                        } else { // rotate
                            const CRot_matrix& rot = m->GetObject().GetRotate();
                            scale = 1.0 / rot.GetScale_factor();
                            xmat.m[0]=scale*rot.GetRot_11(); xmat.m[1]=scale*rot.GetRot_21(); xmat.m[2]= scale*rot.GetRot_31(); xmat.m[3]=0;
                            xmat.m[4]=scale*rot.GetRot_12(); xmat.m[5]=scale*rot.GetRot_22(); xmat.m[6]= scale*rot.GetRot_32(); xmat.m[7]=0;
                            xmat.m[8]=scale*rot.GetRot_13(); xmat.m[9]=scale*rot.GetRot_23(); xmat.m[10]=scale*rot.GetRot_33(); xmat.m[11]=0;
                            xmat.m[12]=0;                    xmat.m[13]=0;                    xmat.m[14]=0;                     xmat.m[15]=1;
                        }
                        ComposeInto(transformToMaster, xmat, xform);
                        xform = *transformToMaster;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

// override DrawAll so that the graph will be applied to each CoordSet
bool StructureObject::DrawAll(const AtomSet *ignored) const
{
    TESTMSG("drawing StructureObject " << pdbID);
    
    // apply relative transformation if this is a slave structure
    if (!isMaster && parentSet->renderer)
        parentSet->renderer->PushMatrix(transformToMaster);

    // if this is the only StructureObject in this StructureSet, and if this
    // StructureObject has only one CoordSet, and if this CoordSet's AtomSet
    // has multiple ensembles, then draw multiple altConf ensembles
    if (coordSets.size() == 1 && coordSets.front()->atomSet->ensembles.size() > 1 &&
        parentSet->objects.size() == 1) {
        AtomSet *atomSet = coordSets.front()->atomSet;
        AtomSet::EnsembleList::iterator e, ee=atomSet->ensembles.end();
        for (e=atomSet->ensembles.begin(); e!=ee; e++) {
            TESTMSG("drawing multiple altConf ensemble '" << **e << '\'');
            atomSet->SetActiveEnsemble(*e);
            if (!graph->DrawAll(coordSets.front()->atomSet)) return true;
        }

    // otherwise, loop through all CoordSets using default altConf ensemble
    } else {
        TESTMSG("drawing default altConf ensemble");
        CoordSetList::const_iterator c, ce=coordSets.end();
        for (c=coordSets.begin(); c!=ce; c++) {
            (*c)->atomSet->SetActiveEnsemble(NULL);
            if (!(graph->DrawAll((*c)->atomSet))) return true;
        }
    }
    // revert GL matrix
    if (!isMaster && parentSet->renderer)
        parentSet->renderer->PopMatrix();

    return true;
}

END_SCOPE(Cn3D)
