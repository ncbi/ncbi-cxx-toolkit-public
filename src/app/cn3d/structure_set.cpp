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
#include <objects/mmdb2/Model_coordinate_set.hpp>
#include <objects/mmdb2/Coordinates.hpp>

#include "cn3d/structure_set.hpp"
#include "cn3d/atom_set.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

StructureSet::StructureSet(const CNcbi_mime_asn1& mime) :
	StructureBase(NULL)
{
    StructureObject *object;
    
    // create StructureObjects from (list of) biostruc
    if (mime.IsStrucseq()) { 
        object = new StructureObject(this, mime.GetStrucseq().GetStructure(), true);
        objects.push_back(object);

    } else if (mime.IsStrucseqs()) {
        object = new StructureObject(this, mime.GetStrucseqs().GetStructure(), true);
        objects.push_back(object);

    } else if (mime.IsAlignstruc()) {
        object = new StructureObject(this, mime.GetAlignstruc().GetMaster(), true);
        objects.push_back(object);
        const CBiostruc_align::TSlaves& slaves = mime.GetAlignstruc().GetSlaves();
		CBiostruc_align::TSlaves::const_iterator i, e=slaves.end();
        for (i=slaves.begin(); i!=e; i++) {
            object = new StructureObject(this, (*i).GetObject(), false);
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
    TESTMSG("deleting StructureSet");
}

void StructureSet::Draw(void) const
{
    TESTMSG("drawing StructureSet");
}

StructureObject::StructureObject(StructureBase *parent, const CBiostruc& biostruc, bool master) :
    StructureBase(parent), isMaster(master), 
    mmdbID(-1), pdbID("(unknown)")
{
    // get MMDB id
    const CBiostruc_id& id = biostruc.GetId().front().GetObject();
    if (id.IsMmdb_id())
        mmdbID = id.GetMmdb_id().Get();
    else
        ERR_POST(Warning << "expecting MMDB ID as first id item in biostruc");
    TESTMSG("MMDB id " << mmdbID);

    // get PDB id
    if (biostruc.IsSetDescr()) {
        const CBiostruc_descr& descr = biostruc.GetDescr().front().GetObject();
        if (descr.IsName())
            pdbID = descr.GetName();
        else
            ERR_POST(Warning << "biostruc " << mmdbID
                        << ": expecting PDB name as first descr item");
    }
    TESTMSG("PDB id " << pdbID);

    // get atom coordinates
    if (biostruc.IsSetModel()) {

        // iterate SEQUENCE OF Biostruc-model
        CBiostruc::TModel::const_iterator i, ie=biostruc.GetModel().end();
        for (i=biostruc.GetModel().begin(); i!=ie; i++) {
            const CBiostruc_model& models = (*i).GetObject();
            if (models.IsSetModel_coordinates()) {
                const CBiostruc_model::TModel_coordinates&
                    modelCoords = models.GetModel_coordinates();

                // iterate SEQUENCE OF Model-coordinate-set
                CBiostruc_model::TModel_coordinates::const_iterator j, je=modelCoords.end();
                for (j=modelCoords.begin(); j!=je; j++) {
                    const CModel_coordinate_set::C_Coordinates& 
                        coordSet = (*j).GetObject().GetCoordinates();
                    if (coordSet.IsLiteral() && coordSet.GetLiteral().IsAtomic()) {
                        AtomSet *atomSet =
                            new AtomSet(this, coordSet.GetLiteral().GetAtomic());
                        atomSets.push_back(atomSet);
                        break;
                    }
                    // will eventually unpack 3d-object coordinates here
                }
            }
        }
    }
}

StructureObject::~StructureObject(void)
{
    TESTMSG("deleting StructureObject " << pdbID);
}

void StructureObject::Draw(void) const
{
    TESTMSG("drawing StructureObject " << pdbID);
}

END_SCOPE(Cn3D)

