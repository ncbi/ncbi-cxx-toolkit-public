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

#include "cn3d/structure_set.hpp"
#include "cn3d/coord_set.hpp"
#include "cn3d/chemical_graph.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

StructureSet::StructureSet(const CNcbi_mime_asn1& mime) :
    StructureBase(NULL, false)
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

bool StructureSet::Draw(void) const
{
    TESTMSG("drawing StructureSet");
    return true;
}

const int StructureObject::NO_MMDB_ID = -1;

StructureObject::StructureObject(StructureBase *parent, const CBiostruc& biostruc, bool master) :
    StructureBase(parent), isMaster(master), mmdbID(NO_MMDB_ID)
{
    // get MMDB id
    CBiostruc::TId::const_iterator j, je=biostruc.GetId().end();
    for (j=biostruc.GetId().begin(); j!=je; j++) {
        if ((*j).GetObject().IsMmdb_id()) {
            mmdbID = (*j).GetObject().GetMmdb_id().Get();
            break;
        }
    }
    TESTMSG("MMDB id " << mmdbID);

    // get PDB id
    if (biostruc.IsSetDescr()) {
        CBiostruc::TDescr::const_iterator k, ke=biostruc.GetDescr().end();
        for (k=biostruc.GetDescr().begin(); k!=ke; k++) {
            if ((*k).GetObject().IsName()) {
                pdbID = (*k).GetObject().GetName();
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
            if ((*i).GetObject().IsSetModel_coordinates()) {
                CoordSet *coordSet =
                    new CoordSet(this, (*i).GetObject().GetModel_coordinates());
                coordSets.push_back(coordSet);
            }
        }
    }

    // get bonds
    graph = new ChemicalGraph(this, biostruc.GetChemical_graph());
}

bool StructureObject::Draw(void) const
{
    TESTMSG("drawing StructureObject " << pdbID);
    return true;
}

END_SCOPE(Cn3D)

