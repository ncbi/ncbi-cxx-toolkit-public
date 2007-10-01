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
                                                 m_KeepParen(false),
                                                 m_HIVCloneIsolateRule(ePreferClone)

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
    
    m_UseModifierLabels = orig->GetUseModifierLabels();
    m_KeepCountryText = orig->GetKeepCountryText();
    m_ExcludeSpOrgs = orig->GetExcludeSpOrgs();
    m_KeepParen = orig->GetKeepParen();
    m_HIVCloneIsolateRule = orig->GetHIVCloneIsolateRule();
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
    for (k = 0; k < m_GroupList.size() && ! found; k++) {
        if (m_GroupList[k]->SourceDescBelongsHere(new_org, this)) {
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
    m_SubSources.push_back(st);
    
    for (k = 0; k < orig_group_num; k++) {
        
        new_grp = m_GroupList[k]->RemoveNonMatchingDescriptions (this);
        while (new_grp != NULL) {
            m_GroupList.push_back(new_grp);
            new_grp = new_grp->RemoveNonMatchingDescriptions(this);
        }
    }
}


void CAutoDefModifierCombo::AddOrgMod(COrgMod::ESubtype st)
{
    unsigned int k, orig_group_num;
    CAutoDefSourceGroup *new_grp;
    
    orig_group_num = m_GroupList.size();
    m_OrgMods.push_back(st);
   
    for (k = 0; k < orig_group_num; k++) {
        
        new_grp = m_GroupList[k]->RemoveNonMatchingDescriptions (this);
        while (new_grp != NULL) {
            m_GroupList.push_back(new_grp);
            new_grp = new_grp->RemoveNonMatchingDescriptions(this);
        }
    }
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
    } else if (m_UseModifierLabels) {
        label = CAutoDefAvailableModifier::GetSubSourceLabel (st);
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
    } else if (m_UseModifierLabels) {
        label = CAutoDefAvailableModifier::GetOrgModLabel(st);
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


bool CAutoDefModifierCombo::x_AddSubsourceString (string &source_description, const CBioSource& bsrc, CSubSource::ESubtype st)
{
    bool         used = false;
    string       val;

    ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
        if ((*subSrcI)->GetSubtype() == st) {
            source_description += x_GetSubSourceLabel (st);

            source_description += " ";
            string val = (*subSrcI)->GetName();
            // truncate value at first semicolon
            unsigned int pos = NStr::Find(val, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                val = val.substr(0, pos);
            }
                    
            // if country and not keeping text after colon, truncate after colon
            if (st == CSubSource::eSubtype_country
                && ! m_KeepCountryText) {
                pos = NStr::Find(val, ":");
                if (pos != NCBI_NS_STD::string::npos) {
                    val = val.substr(0, pos);
                }
            }
            source_description += val;
            used = true;
        }
    }
    return used;
}


bool CAutoDefModifierCombo::x_AddOrgModString (string &source_description, const CBioSource& bsrc, COrgMod::ESubtype st)
{
    bool         used = false;
    string       val;

    ITERATE (COrgName::TMod, modI, bsrc.GetOrg().GetOrgname().GetMod()) {
        if ((*modI)->GetSubtype() == st) {

            string val = (*modI)->GetSubname();
            // truncate value at first semicolon
            unsigned int pos = NStr::Find(val, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                val = val.substr(0, pos);
            }
            if (st == COrgMod::eSubtype_specimen_voucher && NStr::StartsWith (val, "personal:")) {
                val = val.substr(9);
            }
            // If modifier is one of the following types and the value already appears in the tax Name,
            // don't use in the organism description
            if ((st == COrgMod::eSubtype_strain
                 || st == COrgMod::eSubtype_variety
                 || st == COrgMod::eSubtype_sub_species
                 || st == COrgMod::eSubtype_forma
                 || st == COrgMod::eSubtype_forma_specialis
                 || st == COrgMod::eSubtype_pathovar
                 || st == COrgMod::eSubtype_specimen_voucher)
                && NStr::Find (bsrc.GetOrg().GetTaxname(), val) != NCBI_NS_STD::string::npos) {
                // can't use this
            } else {
                source_description += x_GetOrgModLabel(st);

                source_description += " ";
                source_description += val;
                used = true;
            }
        }
    }
    return used;
}


bool CAutoDefModifierCombo::HasTrickyHIV()
{
    bool has_tricky = false;
    
    for (unsigned int k = 0; k < m_GroupList.size() && !has_tricky; k++) {
        has_tricky = m_GroupList[k]->HasTrickyHIV();
    }
    return has_tricky;
}


unsigned int CAutoDefModifierCombo::x_AddHIVModifiers (string &source_description, const CBioSource& bsrc)
{
    unsigned int mods_used = 0;
    string   clone_text = "";
    string   isolate_text = "";
    bool     src_has_clone = false;
    bool     src_has_isolate = false;
    
    if (!NStr::Equal (source_description, "HIV-1")
        &&  !NStr::Equal (source_description, "HIV-2")) {
        return mods_used;
    }
    
    if (!HasSubSource (CSubSource::eSubtype_country)) {
        if (x_AddSubsourceString (source_description, bsrc, CSubSource::eSubtype_country)) {
            mods_used++;
        }
    }
    
    src_has_clone = x_AddSubsourceString(clone_text, bsrc, CSubSource::eSubtype_clone);
    src_has_isolate = x_AddOrgModString (isolate_text, bsrc, COrgMod::eSubtype_isolate);
    
    if ((HasSubSource (CSubSource::eSubtype_clone) && src_has_clone)
        || (HasOrgMod (COrgMod::eSubtype_isolate) && src_has_isolate)) {
        // no additional changes - isolate and clone rule taken care of
    } else {
        if ( ! HasOrgMod (COrgMod::eSubtype_isolate) && src_has_isolate
            && (m_HIVCloneIsolateRule == ePreferIsolate
                || m_HIVCloneIsolateRule == eWantBoth
                || !src_has_clone)) {
            x_AddOrgModString (source_description, bsrc, COrgMod::eSubtype_isolate);
            mods_used++;
        }
        if (! HasSubSource(CSubSource::eSubtype_clone) && src_has_clone
            && (m_HIVCloneIsolateRule == ePreferClone
                || m_HIVCloneIsolateRule == eWantBoth
                || !src_has_isolate)) {
            x_AddSubsourceString (source_description, bsrc, CSubSource::eSubtype_clone);
            mods_used++;
        }
    }    
    
    return mods_used;
}


bool IsSpName (string taxname)
{
    bool is_sp_name = false;
    unsigned int pos = NStr::Find(taxname, " sp. ");
    if (pos != NCBI_NS_STD::string::npos
        && (pos < 2 || !NStr::StartsWith(taxname.substr(pos - 2), "f."))) {
        is_sp_name = true;
    }
    return is_sp_name;
}


bool CAutoDefModifierCombo::GetDefaultExcludeSp ()
{
    bool default_exclude = true;
    
    for (unsigned int k = 0; k < m_GroupList.size() && default_exclude; k++) {
        default_exclude = m_GroupList[k]->GetDefaultExcludeSp ();
    }
    return default_exclude;
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
    
    mods_used += x_AddHIVModifiers (source_description, bsrc);

    if (bsrc.CanGetOrigin() && bsrc.GetOrigin() == CBioSource::eOrigin_mut) {
        source_description = "Mutant " + source_description;
    }
    
    if (bsrc.CanGetSubtype()) {
        for (k = 0; k < GetNumSubSources() && (m_MaxModifiers == 0 || mods_used < m_MaxModifiers); k++) {
            if (x_AddSubsourceString (source_description, bsrc, GetSubSource(k))) {
                mods_used++;
            }
        }
    }

    if (bsrc.CanGetOrg() && bsrc.GetOrg().CanGetOrgname() && bsrc.GetOrg().GetOrgname().IsSetMod()) {
        for (k = 0; k < GetNumOrgMods() && (m_MaxModifiers == 0 || mods_used < m_MaxModifiers); k++) {
            if (x_AddOrgModString (source_description, bsrc, GetOrgMod(k))) {
                mods_used++;
            }
        }
    }
    
    return source_description;
}


END_SCOPE(objects)
END_NCBI_SCOPE
