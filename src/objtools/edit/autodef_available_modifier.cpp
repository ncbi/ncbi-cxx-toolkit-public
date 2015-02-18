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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <objtools/edit/autodef.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CAutoDefAvailableModifier::CAutoDefAvailableModifier() 
                    : m_IsOrgMod(true), 
                      m_SubSrcType(CSubSource::eSubtype_other),
                      m_OrgModType(COrgMod::eSubtype_other),
                      m_AllUnique (true), 
                      m_AllPresent (true), 
                      m_IsUnique(true)
{
    m_ValueList.clear();
}


CAutoDefAvailableModifier::CAutoDefAvailableModifier(unsigned int type, bool is_orgmod)
                    : m_IsOrgMod(is_orgmod), 
                      m_SubSrcType(CSubSource::eSubtype_other),
                      m_OrgModType(COrgMod::eSubtype_other),
                      m_AllUnique (true),
                      m_AllPresent (true), 
                      m_IsUnique(true), 
                      m_IsRequested (false)
{
    m_ValueList.clear();
    if (is_orgmod) {
        m_OrgModType = (COrgMod::ESubtype) type;
    } else {
        m_SubSrcType = (CSubSource::ESubtype) type;
    }
}
    
CAutoDefAvailableModifier::CAutoDefAvailableModifier (const CAutoDefAvailableModifier& copy)
{
    m_IsOrgMod = copy.IsOrgMod();
    if (m_IsOrgMod) {
        m_OrgModType = copy.GetOrgModType();
    } else {
        m_SubSrcType = copy.GetSubSourceType();
    }
    m_AllUnique = copy.AllUnique();
    m_AllPresent = copy.AllPresent();
    m_IsUnique = copy.IsUnique();
    m_IsRequested = copy.IsRequested();
    m_ValueList.clear();
    if (copy.m_ValueList.size() > 0) {
        ValueFound(copy.m_ValueList[0]);
    }
}


CAutoDefAvailableModifier::~CAutoDefAvailableModifier()
{
}


/**
    CAutoDefAvailableModifier comparator
    to sort the set properly.
    subsources first, then orgmods
*/
bool CAutoDefAvailableModifier::operator<(const CAutoDefAvailableModifier& rhs) const
{
    unsigned int this_rank = GetRank();
    unsigned int rhs_rank = rhs.GetRank();
    
    if (this_rank != rhs_rank) {
        return this_rank < rhs_rank;
    } else if (rhs.IsOrgMod()) {
        if (m_IsOrgMod) {
            return m_OrgModType < rhs.GetOrgModType();
        } else {
            return true;
        }
    } else {
        if (m_IsOrgMod) {
            return false;
        } else {
            return m_SubSrcType < rhs.GetSubSourceType();
        }
    }
}


bool CAutoDefAvailableModifier::operator==(const CAutoDefAvailableModifier& rhs) const
{
    if (m_IsOrgMod) {
        if (rhs.IsOrgMod()) {
            return m_OrgModType == rhs.GetOrgModType();
        } else {
            return false;
        }
    } else if (rhs.IsOrgMod()) {
        return false;
    } else {
        return m_SubSrcType == rhs.GetSubSourceType();
    }
}

   
void CAutoDefAvailableModifier::SetOrgModType(COrgMod::ESubtype orgmod_type)
{
    m_IsOrgMod = true;
    m_OrgModType = orgmod_type;
}


void CAutoDefAvailableModifier::SetSubSourceType(CSubSource::ESubtype subsrc_type)
{
    m_IsOrgMod = false;
    m_SubSrcType = subsrc_type;
}


void CAutoDefAvailableModifier::ValueFound(string val_found)
{
    bool found = false;
    if (NStr::Equal("", val_found)) {
        m_AllPresent = false;
    } else {
        for (unsigned int k = 0; k < m_ValueList.size(); k++) {
            if (NStr::Equal(val_found, m_ValueList[k])) {
                m_AllUnique = false;
                found = true;
                break;
            }
        }
        if (!found && m_ValueList.size() > 0) {
            m_IsUnique = false;
        }
        if (!found) {
            m_ValueList.push_back(val_found);
        }
    }
}


void CAutoDefAvailableModifier::FirstValue(string& first_val)
{
    if (m_ValueList.size() > 0) {
        first_val = m_ValueList[0];
    } else {
        first_val = "";
    }
}


bool CAutoDefAvailableModifier::AnyPresent() const
{
    if (m_ValueList.size() > 0) {
        return true;
    } else {
        return false;
    }
}


unsigned int CAutoDefAvailableModifier::GetRank() const
{
    if (m_IsOrgMod) {
        if (m_OrgModType == COrgMod::eSubtype_strain) {
            return 3;
        } else if (m_OrgModType == COrgMod::eSubtype_isolate) {
            return 5;
        } else if (m_OrgModType == COrgMod::eSubtype_cultivar) {
            return 7;
        } else if (m_OrgModType == COrgMod::eSubtype_specimen_voucher) {
            return 8;
        } else if (m_OrgModType == COrgMod::eSubtype_ecotype) {
            return 9;
        } else if (m_OrgModType == COrgMod::eSubtype_type) {
            return 10;
        } else if (m_OrgModType == COrgMod::eSubtype_serotype) {
            return 11;
        } else if (m_OrgModType == COrgMod::eSubtype_authority) {
            return 12;
        } else if (m_OrgModType == COrgMod::eSubtype_breed) {
            return 13;
        }
    } else {
        if (m_SubSrcType == CSubSource::eSubtype_transgenic) {
            return 0;
        } else if (m_SubSrcType == CSubSource::eSubtype_plasmid_name) {
            return 1;
         } else if (m_SubSrcType == CSubSource::eSubtype_endogenous_virus_name)  {
            return 2;
        } else if (m_SubSrcType == CSubSource::eSubtype_clone) {
            return 4;
        } else if (m_SubSrcType == CSubSource::eSubtype_haplotype) {
            return 6;
        }
    }
    return 50;
}


string CAutoDefAvailableModifier::GetSubSourceLabel (CSubSource::ESubtype st)
{
    string label = "";
    
    switch (st) {
        case CSubSource::eSubtype_endogenous_virus_name:
            label = "endogenous virus";
            break;
        case CSubSource::eSubtype_transgenic:
            label = "transgenic";
            break;
        case CSubSource::eSubtype_plasmid_name:
            label = "plasmid";
            break;
        case CSubSource::eSubtype_country:
            label = "country";
            break;
        case CSubSource::eSubtype_chromosome:
            label = "chromosome";
            break;
        case CSubSource::eSubtype_clone:
            label = "clone";
            break;
        case CSubSource::eSubtype_subclone:
            label = "subclone";
            break;
        case CSubSource::eSubtype_haplotype:
            label = "haplotype";
            break;
        case CSubSource::eSubtype_genotype:
            label = "genotype";
            break;
        case CSubSource::eSubtype_sex:
            label = "sex";
            break;
        case CSubSource::eSubtype_cell_line:
            label = "cell line";
            break;
        case CSubSource::eSubtype_cell_type:
            label = "cell type";
            break;
        case CSubSource::eSubtype_tissue_type:
            label = "tissue type";
            break;
        case CSubSource::eSubtype_clone_lib:
            label = "clone lib";
            break;
        case CSubSource::eSubtype_dev_stage:
            label = "dev stage";
            break;
        case CSubSource::eSubtype_frequency:
            label = "frequency";
            break;
        case CSubSource::eSubtype_germline:
            label = "germline";
            break;
        case CSubSource::eSubtype_lab_host:
            label = "lab host";
            break;
        case CSubSource::eSubtype_pop_variant:
            label = "pop variant";
            break;
        case CSubSource::eSubtype_tissue_lib:
            label = "tissue lib";
            break;
        case CSubSource::eSubtype_transposon_name:
            label = "transposon";
            break;
        case CSubSource::eSubtype_insertion_seq_name:
            label = "insertion sequence";
            break;
        case CSubSource::eSubtype_plastid_name:
            label = "plastid";
            break;
        case CSubSource::eSubtype_segment:
            label = "segment";
            break;
        case CSubSource::eSubtype_isolation_source:
            label = "isolation source";
            break;
        case CSubSource::eSubtype_lat_lon:
            label = "lat lon";
            break;
        case CSubSource::eSubtype_altitude:
            label = "altitude";
            break;
        case CSubSource::eSubtype_collection_date:
            label = "collection date";
            break;
        case CSubSource::eSubtype_collected_by:
            label = "collected by";
            break;
        case CSubSource::eSubtype_identified_by:
            label = "identified by";
            break;
        case CSubSource::eSubtype_other:
            label = "subsource note";
            break;
        default:
            label = "";
            break;
    }
    return label;
}


string CAutoDefAvailableModifier::GetOrgModLabel(COrgMod::ESubtype st)
{
    string label = "";
    switch (st) {
        case COrgMod::eSubtype_nat_host:
            label = "specific host";
            break;
        case COrgMod::eSubtype_strain:
            label = "strain";
            break;
        case COrgMod::eSubtype_substrain:
            label = "substrain";
            break;
        case COrgMod::eSubtype_type:
            label = "type";
            break;
        case COrgMod::eSubtype_subtype:
            label = "subtype";
            break;
        case COrgMod::eSubtype_variety:
            label = "variety";
            break;
        case COrgMod::eSubtype_serotype:
            label = "serotype";
            break;
        case COrgMod::eSubtype_serogroup:
            label = "serogroup";
            break;
        case COrgMod::eSubtype_serovar:
            label = "serovar";
            break;
        case COrgMod::eSubtype_cultivar:
            label = "cultivar";
            break;
        case COrgMod::eSubtype_pathovar:
            label = "pathovar";
            break;
        case COrgMod::eSubtype_chemovar:
            label = "chemovar";
            break;
        case COrgMod::eSubtype_biovar:
            label = "biovar";
            break;
        case COrgMod::eSubtype_biotype:
            label = "biotype";
            break;
        case COrgMod::eSubtype_group:
            label = "group";
            break;
        case COrgMod::eSubtype_subgroup:
            label = "subgroup";
            break;
        case COrgMod::eSubtype_isolate:
            label = "isolate";
            break;
        case COrgMod::eSubtype_common:
            label = "common name";
            break;
        case COrgMod::eSubtype_acronym:
            label = "acronym";
            break;
        case COrgMod::eSubtype_sub_species:
            label = "subspecies";
            break;
        case COrgMod::eSubtype_specimen_voucher:
            label = "voucher";
            break;
        case COrgMod::eSubtype_authority:
            label = "authority";
            break;
        case COrgMod::eSubtype_forma:
            label = "forma";
            break;
        case COrgMod::eSubtype_forma_specialis:
            label = "forma specialis";
            break;
        case COrgMod::eSubtype_ecotype:
            label = "ecotype";
            break;
        case COrgMod::eSubtype_synonym:
            label = "synonym";
            break;
        case COrgMod::eSubtype_anamorph:
            label = "anamorph";
            break;
        case COrgMod::eSubtype_teleomorph:
            label = "teleomorph";
            break;
        case COrgMod::eSubtype_breed:
            label = "breed";
            break;
        case COrgMod::eSubtype_gb_acronym:
            label = "acronym";
            break;
        case COrgMod::eSubtype_gb_anamorph:
            label = "anamorph";
            break;
        case COrgMod::eSubtype_gb_synonym:
            label = "synonym";
            break;
        case COrgMod::eSubtype_other:
            label = "organism note";
            break;
        default:
            label = COrgMod::GetSubtypeName(st);
            break;
    }
    return label;
}


string CAutoDefAvailableModifier::Label() const
{
    if (m_IsOrgMod) {
        return GetOrgModLabel(m_OrgModType);
    } else {
        return GetSubSourceLabel(m_SubSrcType);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
