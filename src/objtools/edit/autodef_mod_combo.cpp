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
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

#include <objtools/edit/autodef_mod_combo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CAutoDefModifierCombo::CAutoDefModifierCombo() : m_UseModifierLabels(false),
                                                 m_MaxModifiers(0),
                                                 m_KeepCountryText(false),
                                                 m_ExcludeSpOrgs(false),
                                                 m_ExcludeCfOrgs(false),
                                                 m_ExcludeNrOrgs(false),
                                                 m_ExcludeAffOrgs(false),
                                                 m_KeepParen(true),
                                                 m_KeepAfterSemicolon(false),
                                                 m_HIVCloneIsolateRule(ePreferClone)

{
    m_SubSources.clear();
    m_OrgMods.clear();

    m_GroupList.clear();
    m_Modifiers.clear();
}


CAutoDefModifierCombo::CAutoDefModifierCombo(CAutoDefModifierCombo *orig) 
{
    _ASSERT (orig);
    m_SubSources.clear();
    m_OrgMods.clear();
    
    m_GroupList.clear();
    m_Modifiers.clear();

    ITERATE (TGroupListVector, it, orig->GetGroupList()) {
        CAutoDefSourceGroup * g = new CAutoDefSourceGroup(*it);
        m_GroupList.push_back (g);
    }
    ITERATE (CAutoDefSourceDescription::TModifierVector, it, orig->GetModifiers()) {
        m_Modifiers.push_back (CAutoDefSourceModifierInfo(*it));
    }
    
    unsigned int k;
    for (k = 0; k < orig->GetNumSubSources(); k++) {
        m_SubSources.push_back(orig->GetSubSource(k));
    }
    
    for (k = 0; k < orig->GetNumOrgMods(); k++) {
        m_OrgMods.push_back(orig->GetOrgMod(k));
    }
    
    m_UseModifierLabels = orig->GetUseModifierLabels();
    m_KeepCountryText = orig->GetKeepCountryText();
    m_ExcludeSpOrgs = orig->GetExcludeSpOrgs();
    m_ExcludeCfOrgs = orig->GetExcludeCfOrgs();
    m_ExcludeNrOrgs = orig->GetExcludeNrOrgs();
    m_ExcludeAffOrgs = orig->GetExcludeAffOrgs();
    m_KeepParen = orig->GetKeepParen();
    m_KeepAfterSemicolon = orig->GetKeepAfterSemicolon();
    m_HIVCloneIsolateRule = orig->GetHIVCloneIsolateRule();
    m_MaxModifiers = orig->GetMaxModifiers();
}


CAutoDefModifierCombo::~CAutoDefModifierCombo()
{
    for (unsigned int k = 0; k < m_GroupList.size(); k++) {
        delete m_GroupList[k];
    }
}


unsigned int CAutoDefModifierCombo::GetNumGroups()
{
    return m_GroupList.size();
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


void CAutoDefModifierCombo::AddSource(const CBioSource& bs, string feature_clauses) 
{
    CAutoDefSourceDescription src(bs, feature_clauses);
    bool found = false;

    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() > 0
            && src.Compare (*((*it)->GetSrcList().begin())) == 0) {
            (*it)->AddSource (&src);
            found = true;
        }
    }
    if (!found) {
        CAutoDefSourceGroup * g = new CAutoDefSourceGroup();
        g->AddSource (&src);
        m_GroupList.push_back (g);
    }
}


void CAutoDefModifierCombo::AddSubsource(CSubSource::ESubtype st, bool even_if_not_uniquifying)
{
    AddQual(false, st, even_if_not_uniquifying);
}


void CAutoDefModifierCombo::AddOrgMod(COrgMod::ESubtype st, bool even_if_not_uniquifying)
{
    AddQual (true, st, even_if_not_uniquifying);
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
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_bio_material, true));
    modifier_list.push_back(CAutoDefAvailableModifier(COrgMod::eSubtype_culture_collection, true));
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
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_altitude, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_collection_date, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_collected_by, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_identified_by, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_mating_type, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_linkage_group, false));
    modifier_list.push_back(CAutoDefAvailableModifier(CSubSource::eSubtype_haplogroup, false));
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
        string::size_type pos = NStr::Find(tax_name, "(");
        if (pos != NCBI_NS_STD::string::npos) {
            tax_name = tax_name.substr(0, pos);
            NStr::TruncateSpacesInPlace(tax_name);
        }
    }
}


bool CAutoDefModifierCombo::x_AddSubsourceString (string &source_description, const CBioSource& bsrc, CSubSource::ESubtype st)
{
    bool         used = false;

    if (!bsrc.IsSetSubtype()) {
        return false;
    }
    ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
        if ((*subSrcI)->GetSubtype() == st) {
            source_description += x_GetSubSourceLabel (st);

            string val = (*subSrcI)->GetName();
            // truncate value at first semicolon
			      if (!m_KeepAfterSemicolon) {
				        string::size_type pos = NStr::Find(val, ";");
				        if (pos != NCBI_NS_STD::string::npos) {
					          val = val.substr(0, pos);
				        }
			      }
                    
            // if country and not keeping text after colon, truncate after colon
            if (st == CSubSource::eSubtype_country
                && ! m_KeepCountryText) {
                string::size_type pos = NStr::Find(val, ":");
                if (pos != NCBI_NS_STD::string::npos) {
                    val = val.substr(0, pos);
                }
            } else if (st == CSubSource::eSubtype_plasmid_name && NStr::EqualNocase(val, "unnamed")) {
                val = "";
            }
            if (!NStr::IsBlank(val)) {
                source_description += " " + val;
            }
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
			if (!m_KeepAfterSemicolon) {
				string::size_type pos = NStr::Find(val, ";");
				if (pos != NCBI_NS_STD::string::npos) {
					val = val.substr(0, pos);
				}
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
                && NStr::Find (bsrc.GetOrg().GetTaxname(), " " + val) != NCBI_NS_STD::string::npos) {
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


bool CAutoDefModifierCombo::GetDefaultExcludeSp ()
{
    bool default_exclude = true;

    for (unsigned int k = 0; k < m_GroupList.size() && default_exclude; k++) {
        default_exclude = m_GroupList[k]->GetDefaultExcludeSp ();
    }
    return default_exclude;
}


unsigned int CAutoDefModifierCombo::x_AddRequiredSubSourceModifiers (string& description, const CBioSource& bsrc)
{
    unsigned int num_added = 0;

    if (x_AddSubsourceString (description, bsrc, CSubSource::eSubtype_endogenous_virus_name)) {
        num_added++;
    }
    if (x_AddSubsourceString (description, bsrc, CSubSource::eSubtype_plasmid_name)) {
        num_added++;
    }
    if (x_AddSubsourceString (description, bsrc, CSubSource::eSubtype_transgenic)) {
        num_added++;
    }
              
    return num_added;            
}


string CAutoDefModifierCombo::GetSourceDescriptionString (const CBioSource& bsrc) 
{
    unsigned int k;
    string       source_description = "";
    unsigned int mods_used = 0;
    
    /* start with tax name */
    source_description += bsrc.GetOrg().GetTaxname();
    x_CleanUpTaxName(source_description);

    /* should this organism be excluded? */
    if (m_ExcludeSpOrgs) {
        string::size_type pos = NStr::Find(source_description, " sp. ");
        if (pos != NCBI_NS_STD::string::npos
            && (pos < 2 || !NStr::StartsWith(source_description.substr(pos - 2), "f."))) {
            return source_description;
        }
    }
    if (m_ExcludeCfOrgs) {
        string::size_type pos = NStr::Find(source_description, " cf. ");
        if (pos != NCBI_NS_STD::string::npos) {
            return source_description;
        }
    }
    if (m_ExcludeNrOrgs) {
        string::size_type pos = NStr::Find(source_description, " nr. ");
        if (pos != NCBI_NS_STD::string::npos) {
            return source_description;
        }
    }
    if (m_ExcludeAffOrgs) {
        string::size_type pos = NStr::Find(source_description, " aff. ");
        if (pos != NCBI_NS_STD::string::npos) {
            return source_description;
        }
    }
    
    mods_used += x_AddRequiredSubSourceModifiers(source_description, bsrc);
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


unsigned int CAutoDefModifierCombo::GetNumUnique () const
{
    unsigned int num = 0;

    ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() == 1) {
            num++;
        }
    }
    return num;
}


unsigned int CAutoDefModifierCombo::GetMaxInGroup () const
{
    unsigned int num = 0;

    ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() > num) {
            num = (*it)->GetSrcList().size();
        }
    }
    return num;
}


/* NOTE - we want to sort combos from most unique organisms to least unique organisms */
/* secondary sort - most groups to least groups */
/* tertiary sort - fewer max orgs in group to most max orgs in group */
/* fourth sort - least mods to most mods */
int CAutoDefModifierCombo::Compare(const CAutoDefModifierCombo& other) const
{
    int rval = 0;
    unsigned int num_this, num_other;

    num_this = GetNumUnique();
    num_other = other.GetNumUnique();
    if (num_this > num_other) {
        rval = -1;
    } else if (num_this < num_other) {
        rval = 1;
    } else {
        num_this = m_GroupList.size();
        num_other = other.GetGroupList().size();
        if (num_this > num_other) {
            rval = -1;
        } else if (num_this < num_other) {
            rval = 1;
        } else {
            num_this = GetMaxInGroup ();
            num_other = other.GetMaxInGroup();
            if (num_this < num_other) {
                rval = -1;
            } else if (num_this > num_other) {
                rval = 1;
            } else {
                num_this = m_Modifiers.size();
                num_other = other.GetModifiers().size();
                if (num_this < num_other) {
                    rval = -1;
                } else if (num_this > num_other) {
                    rval = 1;
                }
            }
        }
    }
    return rval;
}


struct SAutoDefSourceGroupByStrings {
    bool operator()(const CAutoDefSourceGroup& s1,
                    const CAutoDefSourceGroup& s2) const
    {
        return (s1 < s2);
    }
};


bool CAutoDefModifierCombo::AddQual (bool IsOrgMod, int subtype, bool even_if_not_uniquifying)
{
    bool added = false, rval = false;
    vector <CAutoDefSourceGroup *> new_groups;

    new_groups.clear();
    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        added |= (*it)->AddQual (IsOrgMod, subtype, m_KeepAfterSemicolon);
    }

    if (added) {
        NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
            vector <CAutoDefSourceGroup *> tmp = (*it)->RemoveNonMatchingDescriptions();
            while (!tmp.empty()) {
                new_groups.push_back (tmp[tmp.size() - 1]);
                tmp.pop_back();
                rval = true;
            }
        }
    }
    // NOTE - need to put groups from non-matching descriptions and put them in a new_groups list
    // in order to avoid processing them twice
    while (!new_groups.empty()) {
        m_GroupList.push_back (new_groups[new_groups.size() - 1]);
        new_groups.pop_back();
    }

    if (rval || even_if_not_uniquifying) {
        m_Modifiers.push_back (CAutoDefSourceModifierInfo (IsOrgMod, subtype, ""));
        std::sort (m_GroupList.begin(), m_GroupList.end(), SAutoDefSourceGroupByStrings());
        if (IsOrgMod) {
            m_OrgMods.push_back ((COrgMod_Base::ESubtype)subtype);
        } else {
            m_SubSources.push_back ((CSubSource_Base::ESubtype)subtype);
        }
    }
    return rval;
}



bool CAutoDefModifierCombo::RemoveQual (bool IsOrgMod, int subtype)
{
    bool rval = false;

    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        rval |= (*it)->RemoveQual (IsOrgMod, subtype);
    }
    return rval;
}


vector<CAutoDefModifierCombo *> CAutoDefModifierCombo::ExpandByAnyPresent()
{
    CAutoDefSourceDescription::TModifierVector mods;
    vector<CAutoDefModifierCombo *> expanded;

    expanded.clear();
    NON_CONST_ITERATE (TGroupListVector, it, m_GroupList) {
        if ((*it)->GetSrcList().size() == 1) {
            break;
        }
        mods = (*it)->GetModifiersPresentForAny();
        ITERATE (CAutoDefSourceDescription::TModifierVector, mod_it, mods) {
            expanded.push_back (new CAutoDefModifierCombo (this));
            if (!expanded[expanded.size() - 1]->AddQual (mod_it->IsOrgMod(), mod_it->GetSubtype())) {
                expanded.pop_back ();
                RemoveQual(mod_it->IsOrgMod(), mod_it->GetSubtype());
            }
        }
        if (!expanded.empty()) {
            break;
        }
    }
    return expanded;
}


bool CAutoDefModifierCombo::AreFeatureClausesUnique()
{
    vector<string> clauses;

    ITERATE (TGroupListVector, g, m_GroupList) {
        CAutoDefSourceGroup::TSourceDescriptionVector src_list = (*g)->GetSrcList();
        CAutoDefSourceGroup::TSourceDescriptionVector::iterator s = src_list.begin();
        while (s != src_list.end()) {
            clauses.push_back((*s)->GetFeatureClauses());
            s++;
        }
    }
    if (clauses.size() < 2) {
        return true;
    }
    sort (clauses.begin(), clauses.end());
    bool unique = true;
    vector<string>::iterator sit = clauses.begin();
    string prev = *sit;
    sit++;
    while (sit != clauses.end() && unique) {
        if (NStr::Equal(prev, *sit)) {
            unique = false;
        } else {
            prev = *sit;
        }
        sit++;
    }
    return unique;
}


END_SCOPE(objects)
END_NCBI_SCOPE
