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
* Revision 1.1  2000/06/27 20:09:40  thiessen
* initial checkin
*
* ===========================================================================
*/

#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/ncbimime/Biostruc_seqs.hpp>
#include <objects/ncbimime/Biostruc_align.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/mmdb2/Biostruc_model.hpp>
#include <objects/mmdb2/Model_coordinate_set.hpp>
#include <objects/mmdb2/Coordinates.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb1/Atom_id.hpp>
#include <objects/mmdb2/Model_space_points.hpp>
#include <objects/mmdb2/Atomic_temperature_factors.hpp>
#include <objects/mmdb2/Atomic_occupancies.hpp>
#include <objects/mmdb2/Alternate_conformation_ids.hpp>
#include <objects/mmdb2/Alternate_conformation_id.hpp>
#include <objects/mmdb2/Anisotro_temperatu_factors.hpp>
#include <objects/mmdb2/Isotropi_temperatu_factors.hpp>

#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

StructureBase::~StructureBase(void) { }

StructureSet::StructureSet(const CNcbi_mime_asn1& mime)
{
    StructureObject *object;
    
    // create StructureObjects from (list of) biostruc
    if (mime.IsStrucseq()) { 
        object = new StructureObject(mime.GetStrucseq().GetStructure(), true);
        objects.push_back(object);

    } else if (mime.IsStrucseqs()) {
        object = new StructureObject(mime.GetStrucseqs().GetStructure(), true);
        objects.push_back(object);

    } else if (mime.IsAlignstruc()) {
        object = new StructureObject(mime.GetAlignstruc().GetMaster(), true);
        objects.push_back(object);
        const CBiostruc_align::TSlaves& slaves = mime.GetAlignstruc().GetSlaves();
		CBiostruc_align::TSlaves::const_iterator i, e=slaves.end();
        for (i=slaves.begin(); i!=e; i++) {
            object = new StructureObject((*i).GetObject(), false);
            objects.push_back(object);
        }

    } else {
        ERR_POST(Fatal << "Can't (yet) handle that Ncbi-mime-asn1 type");
    }
}

StructureSet::~StructureSet(void)
{
    // delete all objects
    DELETE_ALL(ObjectList, objects);
}

void StructureSet::Draw(void) const
{
    // draw all objects
    TESTMSG("drawing all objects");
    DRAW_ALL(ObjectList, objects);
}

StructureObject::StructureObject(const CBiostruc& biostruc, bool master)
    : isMaster(master)
{
    // get MMDB id
    const CBiostruc_id& id = biostruc.GetId().front().GetObject();
    if (id.IsMmdb_id())
        mmdbId = id.GetMmdb_id().Get();
    else
        ERR_POST(Fatal << "expecting MMDB ID as first id item in biostruc");
    TESTMSG("MMDB id " << mmdbId);

    // get PDB id
    if (biostruc.IsSetDescr()) {
        const CBiostruc_descr& descr = biostruc.GetDescr().front().GetObject();
        if (descr.IsName())
            pdbId = descr.GetName();
        else
            ERR_POST(Fatal << "biostruc " << mmdbId
                        << ": expecting PDB name as first descr item");
    } else pdbId = "(unknown)";
    TESTMSG("PDB id " << pdbId);

    // get atom coordinates
    if (biostruc.IsSetModel()) {
        const CBiostruc::TModel& models = biostruc.GetModel();
        // iterate SEQUENCE OF Biostruc-model
        CBiostruc::TModel::const_iterator i, ie=models.end();
        for (i=models.begin(); i!=ie; i++) {
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
                            new AtomSet(coordSet.GetLiteral().GetAtomic());
                        atomSets.push_back(atomSet);
                        break;
                    }
                    // should also unpack feature coordinates here
                }
            }
        }
    }
}

StructureObject::~StructureObject(void)
{
    // delete all coordinate sets
    DELETE_ALL(AtomSetList, atomSets);
}

void StructureObject::Draw(void) const
{
    // draw all atom sets
    TESTMSG("drawing all atom sets for " << pdbId);
    DRAW_ALL(AtomSetList, atomSets);
}

AtomSet::AtomSet(const CAtomic_coordinates& coords)
{
    nAtoms = coords.GetNumber_of_points();
    TESTMSG("model has " << nAtoms << " atomic coords");

    moleculeID = new int[nAtoms];
    residueID = new int[nAtoms];
    atomID = new int[nAtoms];
    atomList = new Atom[nAtoms];
    if (!moleculeID || !residueID || !atomID || !atomList)
        ERR_POST(Fatal << "AtomSet: out of memory");

    bool haveTemp = coords.IsSetTemperature_factors(),
         haveOccup = coords.IsSetOccupancies(),
         haveAlt = coords.IsSetAlternate_conf_ids();

    // sanity check
    if (coords.GetAtoms().GetMolecule_ids().size()!=nAtoms ||
        coords.GetAtoms().GetResidue_ids().size()!=nAtoms || 
        coords.GetAtoms().GetAtom_ids().size()!=nAtoms ||
        coords.GetSites().GetX().size()!=nAtoms || 
        coords.GetSites().GetY().size()!=nAtoms || 
        coords.GetSites().GetZ().size()!=nAtoms ||
        (haveTemp && 
            ((coords.GetTemperature_factors().IsIsotropic() &&
              coords.GetTemperature_factors().GetIsotropic().GetB().size()!=nAtoms) ||
             (coords.GetTemperature_factors().IsAnisotropic() &&
              (coords.GetTemperature_factors().GetAnisotropic().GetB_11().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_12().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_13().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_22().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_23().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_33().size()!=nAtoms)))) ||
        (haveOccup && coords.GetOccupancies().GetO().size()!=nAtoms) ||
        (haveAlt && coords.GetAlternate_conf_ids().Get().size()!=nAtoms))
        ERR_POST(Fatal << "AtomSet: confused by list length mismatch");

    // atom-pntr iterators
    CAtom_pntrs::TMolecule_ids::const_iterator 
        i_mID = coords.GetAtoms().GetMolecule_ids().begin();
    CAtom_pntrs::TResidue_ids::const_iterator 
        i_rID = coords.GetAtoms().GetResidue_ids().begin();
    CAtom_pntrs::TAtom_ids::const_iterator 
        i_aID = coords.GetAtoms().GetAtom_ids().begin();

    // atom site iterators
    double siteScale = static_cast<double>(coords.GetSites().GetScale_factor());
    CModel_space_points::TX::const_iterator i_X = coords.GetSites().GetX().begin();
    CModel_space_points::TY::const_iterator i_Y = coords.GetSites().GetY().begin();
    CModel_space_points::TZ::const_iterator i_Z = coords.GetSites().GetZ().begin();

    // occupancy iterator
    CAtomic_occupancies::TO::const_iterator *i_occup = NULL;
    double occupScale = 0.0;
    if (haveOccup) {
        occupScale = static_cast<double>(coords.GetOccupancies().GetScale_factor());
        i_occup = &(coords.GetOccupancies().GetO().begin());
    }    

    // altConfID iterator
    CAlternate_conformation_ids::Tdata::const_iterator *i_alt = NULL;
    if (haveAlt) i_alt = &(coords.GetAlternate_conf_ids().Get().begin());

    // temperature iterators
    double tempScale = 0.0;
    CIsotropic_temperature_factors::TB::const_iterator *i_tempI = NULL;
    CAnisotropic_temperature_factors::TB_11::const_iterator *i_tempA11 = NULL;
    CAnisotropic_temperature_factors::TB_12::const_iterator *i_tempA12 = NULL;
    CAnisotropic_temperature_factors::TB_13::const_iterator *i_tempA13 = NULL;
    CAnisotropic_temperature_factors::TB_22::const_iterator *i_tempA22 = NULL;
    CAnisotropic_temperature_factors::TB_23::const_iterator *i_tempA23 = NULL;
    CAnisotropic_temperature_factors::TB_33::const_iterator *i_tempA33 = NULL;
    if (haveTemp) {
        if (coords.GetTemperature_factors().IsIsotropic()) {
            tempScale = static_cast<double>
                (coords.GetTemperature_factors().GetIsotropic().GetScale_factor());
            i_tempI = &(coords.GetTemperature_factors().GetIsotropic().GetB().begin());
        } else {
            tempScale = static_cast<double>
                (coords.GetTemperature_factors().GetAnisotropic().GetScale_factor());
            i_tempA11 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_11().begin());
            i_tempA12 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_12().begin());
            i_tempA13 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_13().begin());
            i_tempA22 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_22().begin());
            i_tempA23 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_23().begin());
            i_tempA33 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_33().begin());
        }
    }

    // unpack serial atom data into Atom objects
    for (int i=0; i<nAtoms; i++) {
        moleculeID[i] = (*(i_mID++)).GetObject().Get();
        residueID[i] = (*(i_rID++)).GetObject().Get();
        atomID[i] = (*(i_aID++)).GetObject().Get();
        atomList[i].site.x = (static_cast<double>(*(i_X++)))/siteScale;
        atomList[i].site.y = (static_cast<double>(*(i_Y++)))/siteScale;
        atomList[i].site.z = (static_cast<double>(*(i_Z++)))/siteScale;
        if (haveOccup)
            atomList[i].occupancy = (static_cast<double>(*((*i_occup)++)))/occupScale;
        if (haveAlt)
            atomList[i].altConfID = (*((*i_alt)++)).GetObject().Get()[0];
        if (haveTemp) {
            if (coords.GetTemperature_factors().IsIsotropic()) {
                atomList[i].averageTemperature = (static_cast<double>(*((*i_tempI)++)))/tempScale;
            } else {
                atomList[i].averageTemperature = (
                        (static_cast<double>(*((*i_tempA11)++))) +
                        (static_cast<double>(*((*i_tempA12)++))) +
                        (static_cast<double>(*((*i_tempA13)++))) +
                        (static_cast<double>(*((*i_tempA22)++))) +
                        (static_cast<double>(*((*i_tempA23)++))) +
                        (static_cast<double>(*((*i_tempA33)++)))) / (tempScale * 6.0);
            }
        }
    }
    TESTMSG("first atom: mID " << moleculeID[0] <<
            ", rID " << residueID[0] << ", aID " << atomID[0] <<
            ", x " << atomList[0].site.x << 
            ", y " << atomList[0].site.y << 
            ", z " << atomList[0].site.z <<
			", occup " << atomList[0].occupancy <<
            ", altConfId '" << atomList[0].altConfID << "'" <<
            ", temp " << atomList[0].averageTemperature);
}

AtomSet::~AtomSet(void)
{
    delete moleculeID;
    delete residueID;
    delete atomID;
    delete atomList;
}

void AtomSet::Draw(void) const
{
    // draw all atoms
    TESTMSG("drawing atom set");
    for (int i=0; i<nAtoms; i++) atomList[i].Draw();
}

const Atom* AtomSet::GetAtomPntr(int moleculeID, int residueID, int atomID) const
{
    return NULL;
}

Atom::Atom(void) : 
    averageTemperature(ATOM_NO_TEMPERATURE),
    occupancy(ATOM_NO_OCCUPANCY),
    altConfID(ATOM_NO_ALTCONFID)
{
}

END_SCOPE(Cn3D)

