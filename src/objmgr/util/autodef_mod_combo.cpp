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
#include <corelib/ncbimisc.hpp>
#include <objmgr/util/autodef.hpp>
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

CAutoDefModifierCombo::CAutoDefModifierCombo() : m_UseModifierLabels(false),
                                                 m_KeepCountryText(false),
                                                 m_ExcludeSpOrgs(false),
                                                 m_KeepParen(false)

{
    m_GroupList.clear();
    m_SubSources.clear();
    m_OrgMods.clear();
}


CAutoDefModifierCombo::CAutoDefModifierCombo(CAutoDefModifierCombo *orig) 
{
    _ASSERT (orig);
    m_GroupList.clear();
    m_SubSources.clear();
    m_OrgMods.clear();
    
    unsigned int k;
    for (k = 0; k < orig->GetNumGroups(); k++) {
        m_GroupList.push_back(new CAutoDefSourceGroup(orig->GetSourceGroup(k)));
    }
    
    for (k = 0; k < orig->GetNumSubSources(); k++) {
        m_SubSources.push_back(orig->GetSubSource(k));
    }
    
    for (k = 0; k < orig->GetNumOrgMods(); k++) {
        m_OrgMods.push_back(orig->GetOrgMod(k));
    }
}


CAutoDefModifierCombo::~CAutoDefModifierCombo()
{
}


unsigned int CAutoDefModifierCombo::GetNumGroups()
{
    return m_GroupList.size();
}


CAutoDefSourceGroup *CAutoDefModifierCombo::GetSourceGroup (unsigned int index) 
{
    _ASSERT (index < m_GroupList.size());
    
    return m_GroupList[index];
}


unsigned int CAutoDefModifierCombo::GetNumSubSources()
{
    return m_SubSources.size();
}

   
CSubSource::ESubtype CAutoDefModifierCombo::GetSubSource(unsigned int index)
{
    _ASSERT (index < m_SubSources.size());
    
    return m_SubSources[index];
}


unsigned int CAutoDefModifierCombo::GetNumOrgMods()
{
    return m_OrgMods.size();
}

   
COrgMod::ESubtype CAutoDefModifierCombo::GetOrgMod(unsigned int index)
{
    _ASSERT (index < m_OrgMods.size());
    
    return m_OrgMods[index];
}


bool CAutoDefModifierCombo::HasSubSource(CSubSource::ESubtype st)
{
    for (unsigned int k = 0; k < m_SubSources.size(); k++) {
        if (m_SubSources[k] == st) {
            return true;
        }
    }
    return false;
}


bool CAutoDefModifierCombo::HasOrgMod(COrgMod::ESubtype st)
{
    for (unsigned int k = 0; k < m_OrgMods.size(); k++) {
        if (m_OrgMods[k] == st) {
            return true;
        }
    }
    return false;
}


void CAutoDefModifierCombo::AddSource(const CBioSource& bs) 
{
    CAutoDefSourceDescription *new_org;
    unsigned int k;
    bool         found = false;
    
    new_org = new CAutoDefSourceDescription(bs);
    for (k = 0; k < m_SubSources.size(); k++) {
        new_org->AddSubsource (m_SubSources[k]);
    }
    for (k = 0; k < m_OrgMods.size(); k++) {
        new_org->AddOrgMod (m_OrgMods[k]);
    }
    for (k = 0; k < m_GroupList.size() && ! found; k++) {
        if (m_GroupList[k]->SourceDescBelongsHere(new_org)) {
            m_GroupList[k]->AddSourceDescription(new_org);
            found = true;
        }
    }
    if (!found) {
        CAutoDefSourceGroup *new_grp = new CAutoDefSourceGroup();
        new_grp->AddSourceDescription(new_org);
        m_GroupList.push_back(new_grp);
    }
}


void CAutoDefModifierCombo::AddSubsource(CSubSource::ESubtype st)
{
    unsigned int k, orig_group_num;
    CAutoDefSourceGroup *new_grp;
    
    orig_group_num = m_GroupList.size();
    
    for (k = 0; k < orig_group_num; k++) {
        m_GroupList[k]->AddSubsource (st);
        
        new_grp = m_GroupList[k]->RemoveNonMatchingDescriptions ();
        while (new_grp != NULL) {
            m_GroupList.push_back(new_grp);
            new_grp = new_grp->RemoveNonMatchingDescriptions();
        }
    }
    m_SubSources.push_back(st);
}


void CAutoDefModifierCombo::AddOrgMod(COrgMod::ESubtype st)
{
    unsigned int k, orig_group_num;
    CAutoDefSourceGroup *new_grp;
    
    orig_group_num = m_GroupList.size();
    
    for (k = 0; k < orig_group_num; k++) {
        m_GroupList[k]->AddOrgMod (st);
        
        new_grp = m_GroupList[k]->RemoveNonMatchingDescriptions ();
        while (new_grp != NULL) {
            m_GroupList.push_back(new_grp);
            new_grp = new_grp->RemoveNonMatchingDescriptions();
        }
    }
    m_OrgMods.push_back(st);
}


unsigned int CAutoDefModifierCombo::GetNumUniqueDescriptions()
{
    unsigned int num_unique = 0, k;
 
    for (k = 0; k < m_GroupList.size(); k++) {
        if (m_GroupList[k]->GetNumDescriptions() == 1) {
            num_unique++;
        }
    }
    return num_unique;
}


bool CAutoDefModifierCombo::AllUnique()
{
    bool all_unique = true;
    unsigned int k;
    
    for (k = 0; k < m_GroupList.size() && all_unique; k++) {
        if (m_GroupList[k]->GetNumDescriptions() > 1) {
            all_unique = false;
        }
    }
    return all_unique;
}


void CAutoDefModifierCombo::GetAvailableModifiers (CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    
    // first, set up modifier list with blanks
    modifier_list.clear();
    //note - later come back and rearrange in order of importance
    // add orgmod modifiers
    //dosage, old name, and old lineage are deliberately omitted
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_strain, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_substrain, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_type, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_subtype, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_variety, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_serotype, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_serogroup, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_serovar, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_cultivar, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_pathovar, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_chemovar, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_biovar, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_biotype, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_group, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_subgroup, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_isolate, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_common, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_acronym, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_nat_host, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_sub_species, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_specimen_voucher, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_authority, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_forma, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_forma_specialis, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_ecotype, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_synonym, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_anamorph, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_teleomorph, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_breed, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_gb_acronym, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_gb_anamorph, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_gb_synonym, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_other, true));

    // add subsource modifiers
    // map, fwd_primer_name, fwd_primer_seq, rev_primer_name, and rev_primer_seq are deliberately omitted
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_chromosome, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_clone, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_subclone, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_haplotype, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_genotype, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_sex, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_cell_line, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_cell_type, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_tissue_type, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_clone_lib, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_dev_stage, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_frequency, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_germline, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_rearranged, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_lab_host, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_pop_variant, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_tissue_lib, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_plasmid_name, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_transposon_name, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_insertion_seq_name, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_plastid_name, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_country, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_segment, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_endogenous_virus_name, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_transgenic, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_environmental_sample, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_isolation_source, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_lat_lon, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_collection_date, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_collected_by, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_identified_by, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_other, false));

    for (k = 0; k < m_GroupList.size(); k++) {
        m_GroupList[k]->GetAvailableModifiers(modifier_list);
    }
}


string CAutoDefModifierCombo::x_GetSubSourceLabel (CSubSource::ESubtype st)
{
    string label = "";
    
    if (st == CSubSource::eSubtype_endogenous_virus_name) {
        label = "endogenous virus";
    } else if (st == CSubSource::eSubtype_transgenic) {
        label = "transgenic";
    } else if (st == CSubSource::eSubtype_plasmid_name) {
        label = "plasmid";
    } else if (st == CSubSource::eSubtype_country) {
        label = "from";
        if (m_UseModifierLabels) {
            label += " country";
        }
    } else if (m_UseModifierLabels) {
        switch (st) {
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
                label = "note";
                break;
            default:
                label = "";
                break;
        }
    }
    if (!NStr::IsBlank(label)) {
        label = " " + label;
    }
    return label;
}


string CAutoDefModifierCombo::x_GetOrgModLabel(COrgMod::ESubtype st)
{
    string label = "";
    if (st == COrgMod::eSubtype_nat_host) {
        label = "from";
        if (m_UseModifierLabels) {
            label += " natural host";
        }
    } else if (m_UseModifierLabels) {
        switch (st) {
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
                label = "v";
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
                label = "note";
                break;
            default:
                label = "";
                break;
        }
    }
    if (!NStr::IsBlank(label)) {
        label = " " + label;
    }
    return label;
}


/* This function fixes HIV abbreviations, removes items in parentheses,
 * and trims spaces around the taxonomy name.
 */
void CAutoDefModifierCombo::x_CleanUpTaxName (string &tax_name)
{
    if (NStr::Equal(tax_name, "Human immunodeficiency virus type 1", NStr::eNocase)
        || NStr::Equal(tax_name, "Human immunodeficiency virus 1", NStr::eNocase)) {
        tax_name = "HIV-1";
    } else if (NStr::Equal(tax_name, "Human immunodeficiency virus type 2", NStr::eNocase)
               || NStr::Equal(tax_name, "Human immunodeficiency virus 2", NStr::eNocase)) {
        tax_name = "HIV-2";
    } else if (!m_KeepParen) {
        unsigned int pos = NStr::Find(tax_name, "(");
        if (pos != NCBI_NS_STD::string::npos) {
            tax_name = tax_name.substr(0, pos);
            NStr::TruncateSpacesInPlace(tax_name);
        }
    }
}


string CAutoDefModifierCombo::GetSourceDescriptionString (const CBioSource& bsrc) 
{
    unsigned int k;
    string       source_description = "";
    unsigned int mods_used = 0;
    
    /* start with tax name */
    source_description += bsrc.GetOrg().GetTaxname();
    x_CleanUpTaxName(source_description);
    
    if (m_ExcludeSpOrgs) {
        unsigned int pos = NStr::Find(source_description, " sp. ");
        if (pos != NCBI_NS_STD::string::npos
            && (pos < 2 || !NStr::StartsWith(source_description.substr(pos - 2), "f."))) {
            return source_description;
        }
    }

    if (bsrc.CanGetOrigin() && bsrc.GetOrigin() == CBioSource::eOrigin_mut) {
        source_description = "Mutant " + source_description;
    }
    
    if (bsrc.CanGetSubtype()) {
        for (k = 0; k < GetNumSubSources() && (m_MaxModifiers == 0 || mods_used < m_MaxModifiers); k++) {
            ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == GetSubSource(k)) {
                    source_description += x_GetSubSourceLabel (GetSubSource(k));

                    source_description += " ";
                    string val = (*subSrcI)->GetName();
                    // truncate value at first semicolon
                    unsigned int pos = NStr::Find(val, ";");
                    if (pos != NCBI_NS_STD::string::npos) {
                        val = val.substr(0, pos);
                    }
                    
                    // if country and not keeping text after colon, truncate after colon
                    if (GetSubSource(k) == CSubSource::eSubtype_country
                        && ! m_KeepCountryText) {
                        pos = NStr::Find(val, ":");
                        if (pos != NCBI_NS_STD::string::npos) {
                            val = val.substr(0, pos);
                        }
                    }
                    source_description += val;
                    mods_used ++;
                }
            }
        }
    }
    if (bsrc.CanGetOrg() && bsrc.GetOrg().CanGetOrgname() && bsrc.GetOrg().GetOrgname().CanGetMod()) {
        for (k = 0; k < GetNumOrgMods() && (m_MaxModifiers == 0 || mods_used < m_MaxModifiers); k++) {
            ITERATE (COrgName::TMod, modI, bsrc.GetOrg().GetOrgname().GetMod()) {
                if ((*modI)->GetSubtype() == GetOrgMod(k)) {
                    source_description += x_GetOrgModLabel(GetOrgMod(k));

                    source_description += " ";
                    string val = (*modI)->GetSubname();
                    // truncate value at first semicolon
                    unsigned int pos = NStr::Find(val, ";");
                    if (pos != NCBI_NS_STD::string::npos) {
                        val = val.substr(0, pos);
                    }
                    source_description += val;
                    mods_used++;
                }
            }
        }
    }
    
    return source_description;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.6  2006/05/02 14:12:40  bollin
* moved organism description code out of CAutoDef into CAutoDefModCombo
*
* Revision 1.5  2006/05/02 13:02:48  bollin
* added labels for modifiers, implemented controls for organism description
*
* Revision 1.4  2006/04/20 19:00:59  ucko
* Stop including <objtools/format/context.hpp> -- there's (thankfully!)
* no need to do so, and it confuses SGI's MIPSpro compiler.
*
* Revision 1.3  2006/04/17 17:42:21  ucko
* Drop extraneous and disconcerting inclusion of gui headers.
*
* Revision 1.2  2006/04/17 17:39:13  ucko
* Fix capitalization of header filenames.
* Use standard syntax for enum values.
*
* Revision 1.1  2006/04/17 16:25:05  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

