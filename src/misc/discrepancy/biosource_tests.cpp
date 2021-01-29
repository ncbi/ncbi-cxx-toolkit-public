/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Colleen Bollin, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include <sstream>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/macro/Source_qual.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <util/xregexp/regexp.hpp>

#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(biosource_tests);

#if 0
static unsigned int AutofixBiosrc(TReportObjectList& list, CScope& scope, bool (*call)(CBioSource& src))
{
    unsigned int n = 0;
    for (auto& it : list) {
        if (it->CanAutofix()) {
            const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>(it.GetNCPointer())->GetObject().GetPointer());
            const CSeqdesc* csd = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>(it.GetNCPointer())->GetObject().GetPointer());
            if (sf) {
                if (sf->IsSetData() && sf->GetData().IsBiosrc()) {
                    CRef<CSeq_feat> new_feat(new CSeq_feat());
                    new_feat->Assign(*sf);
                    if (call(new_feat->SetData().SetBiosrc())) {
                        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
                        feh.Replace(*new_feat);
                        n++;
                        dynamic_cast<CDiscrepancyObject*>(it.GetNCPointer())->SetFixed();
                    }
                }
            }
            else if (csd) {
                if (csd->IsSource()) {
                    CSeqdesc* sd = const_cast<CSeqdesc*>(csd);
                    if (call(sd->SetSource())) {
                        n++;
                        dynamic_cast<CDiscrepancyObject*>(it.GetNCPointer())->SetFixed();
                    }
                }
            }
        }
    }
    return n;
}
#endif

// MAP_CHROMOSOME_CONFLICT

DISCREPANCY_CASE(MAP_CHROMOSOME_CONFLICT, BIOSRC, eDisc | eOncaller | eSmart | eFatal, "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    const CBioSource& src = biosrc->GetSource();
    if (src.IsSetSubtype() && context.IsEukaryotic(&src)) {
        bool has_map = false;
        bool has_chromosome = false;
        for (const auto& it : src.GetSubtype()) {
            if (it->IsSetSubtype()) {
                if (it->GetSubtype() == CSubSource::eSubtype_map) {
                    has_map = true;
                }
                else if (it->GetSubtype() == CSubSource::eSubtype_chromosome) {
                    has_chromosome = true;
                    break;
                }
            }
        }
        if (has_map && !has_chromosome) {
            m_Objs["[n] source[s] on eukaryotic sequence[s] [has] map but not chromosome"].Add(*context.SeqdescObjRef(*biosrc)).Fatal();
        }
    }
}


DISCREPANCY_SUMMARIZE(MAP_CHROMOSOME_CONFLICT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INFLUENZA_DATE_MISMATCH

DISCREPANCY_CASE(INFLUENZA_DATE_MISMATCH, BIOSRC, eOncaller, "Influenza Strain/Collection Date Mismatch")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->IsSetSubtype() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod() && biosrc->GetOrg().IsSetTaxname() && NStr::StartsWith(biosrc->GetOrg().GetTaxname(), "Influenza ")) {
            int strain_year = 0;
            int collection_year = 0;
            for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (it->IsSetSubtype() && it->GetSubtype() == COrgMod::eSubtype_strain) {
                    string s = it->GetSubname();
                    size_t pos = s.rfind('/');
                    if (pos == string::npos) {
                        return;
                    }
                    ++pos;
                    while (isspace(s.c_str()[pos])) {
                        ++pos;
                    }
                    size_t len = 0;
                    while (isdigit(s.c_str()[pos + len])) {
                        len++;
                    }
                    if (!len) {
                        return;
                    }
                    strain_year = NStr::StringToInt(s.substr(pos, len));
                    break;
                }
            }
            for (const auto& it : biosrc->GetSubtype()) {
                if (it->IsSetSubtype() && it->GetSubtype() == CSubSource::eSubtype_collection_date) {
                    try {
                        CRef<CDate> date = CSubSource::DateFromCollectionDate(it->GetName());
                        if (date->IsStd() && date->GetStd().IsSetYear()) {
                            collection_year = date->GetStd().GetYear();
                        }
                    }
                    catch (...) {} // LCOV_EXCL_LINE
                    break;
                }
            }
            if (strain_year != collection_year) {
                m_Objs["[n] influenza strain[s] conflict with collection date"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(INFLUENZA_DATE_MISMATCH)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INFLUENZA_QUALS

DISCREPANCY_CASE(INFLUENZA_QUALS, BIOSRC, eOncaller, "Influenza must have strain, host, isolation_source, country, collection_date")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetTaxname() && NStr::StartsWith(biosrc->GetOrg().GetTaxname(), "Influenza ")) {
            bool found_strain = false;
            bool found_host = false;
            bool found_country = false;
            bool found_collection_date = false;
            if (biosrc->IsSetSubtype()) {
                for (const auto& it : biosrc->GetSubtype()) {
                    if (it->IsSetSubtype()) {
                        switch (it->GetSubtype()) {
                        case CSubSource::eSubtype_country:
                            found_country = true;
                            break;
                        case CSubSource::eSubtype_collection_date:
                            found_collection_date = true;
                            break;
                        }
                    }
                }
            }
            if (biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (it->IsSetSubtype()) {
                        switch (it->GetSubtype()) {
                        case COrgMod::eSubtype_strain:
                            found_strain = true;
                            break;
                        case COrgMod::eSubtype_nat_host:
                            found_host = true;
                            break;
                        }
                    }
                }
            }
            if (!found_strain) {
                m_Objs["[n] Influenza biosource[s] [does] not have strain"].Add(*context.BiosourceObjRef(*biosrc));
            }
            if (!found_host) {
                m_Objs["[n] Influenza biosource[s] [does] not have host"].Add(*context.BiosourceObjRef(*biosrc));
            }
            if (!found_country) {
                m_Objs["[n] Influenza biosource[s] [does] not have country"].Add(*context.BiosourceObjRef(*biosrc));
            }
            if (!found_collection_date) {
                m_Objs["[n] Influenza biosource[s] [does] not have collection-date"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(INFLUENZA_QUALS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INFLUENZA_SEROTYPE

DISCREPANCY_CASE(INFLUENZA_SEROTYPE, BIOSRC, eOncaller, "Influenza A virus must have serotype")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetTaxname() && NStr::StartsWith(biosrc->GetOrg().GetTaxname(), "Influenza A virus ") && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            bool found = false;
            for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (it->IsSetSubtype() && it->GetSubtype() == COrgMod::eSubtype_serotype) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_Objs["[n] Influenza A virus biosource[s] [does] not have serotype"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(INFLUENZA_SEROTYPE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INFLUENZA_SEROTYPE_FORMAT

DISCREPANCY_CASE(INFLUENZA_SEROTYPE_FORMAT, BIOSRC, eOncaller, "Influenza A virus serotype must match /^H[1-9]\\d*$|^N[1-9]\\d*$|^H[1-9]\\d*N[1-9]\\d*$|^mixed$/")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetTaxname() && NStr::StartsWith(biosrc->GetOrg().GetTaxname(), "Influenza A virus ")) {
            static CRegexp rx("^H[1-9]\\d*$|^N[1-9]\\d*$|^H[1-9]\\d*N[1-9]\\d*$|^mixed$");
            if (biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (it->IsSetSubtype() && it->GetSubtype() == COrgMod::eSubtype_serotype && !rx.IsMatch(it->GetSubname())) {
                        m_Objs["[n] Influenza A virus serotype[s] [has] incorrect format"].Add(*context.BiosourceObjRef(*biosrc));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(INFLUENZA_SEROTYPE_FORMAT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// UNCULTURED_NOTES

DISCREPANCY_CASE(UNCULTURED_NOTES, BIOSRC, eOncaller | eFatal, "Uncultured Notes")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetSubtype()) {
            for (const auto& it : biosrc->GetSubtype()) {
                if (it->IsSetSubtype() && it->GetSubtype() == CSubSource::eSubtype_other && it->IsSetName() && CSubSource::HasCultureNotes(it->GetName())) {
                    m_Objs["[n] bio-source[s] [has] uncultured note[s]"].Add(*context.BiosourceObjRef(*biosrc)).Fatal();
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNCULTURED_NOTES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_VIRAL_QUALS

const string kMissingViralQualsTop = "[n] virus organism[s] [is] missing required qualifiers";

DISCREPANCY_CASE(MISSING_VIRAL_QUALS, BIOSRC, eOncaller, "Viruses should specify collection-date, country, and specific-host")
{
    const CSeqdesc* src = context.GetBiosource();
    if (context.HasLineage(src ? &src->GetSource() : 0, "Viruses")) {
        for (const CBioSource* biosrc : context.GetBiosources()) {
            bool has_collection_date = false;
            bool has_country = false;
            bool has_specific_host = false;
            if (biosrc->IsSetSubtype()) {
                for (const auto& it : biosrc->GetSubtype()) {
                    if (it->IsSetSubtype()) {
                        if (it->GetSubtype() == CSubSource::eSubtype_collection_date) {
                            has_collection_date = true;
                        }
                        else if (it->GetSubtype() == CSubSource::eSubtype_country) {
                            has_country = true;
                        }
                        if (has_collection_date && has_country) {
                            break;
                        }
                    }
                }
            }
            if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (it->IsSetSubtype() && it->GetSubtype() == COrgMod::eSubtype_nat_host) {
                        has_specific_host = true;
                    }
                }
            }
            if (!has_collection_date || !has_country || !has_specific_host) {
                if (!has_collection_date) {
                    m_Objs[kMissingViralQualsTop]["[n] virus organism[s] [is] missing suggested qualifier collection date"].Ext().Add(*context.BiosourceObjRef(*biosrc));
                }
                if (!has_country) {
                    m_Objs[kMissingViralQualsTop]["[n] virus organism[s] [is] missing suggested qualifier country"].Ext().Add(*context.BiosourceObjRef(*biosrc));
                }
                if (!has_specific_host) {
                    m_Objs[kMissingViralQualsTop]["[n] virus organism[s] [is] missing suggested qualifier specific-host"].Ext().Add(*context.BiosourceObjRef(*biosrc));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MISSING_VIRAL_QUALS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ATCC_CULTURE_CONFLICT

bool HasCultureCollectionForATCCStrain(const COrgName::TMod& mods, const string& strain)
{
    if (NStr::IsBlank(strain)) {
        return true;
    }
    bool found = false;
    for (const auto& m : mods) {
        if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_culture_collection && m->IsSetSubname() && NStr::StartsWith(m->GetSubname(), "ATCC:")) {
            string cmp = m->GetSubname().substr(5);
            NStr::TruncateSpacesInPlace(cmp);
            size_t pos = NStr::Find(cmp, ";");
            if (pos != string::npos) {
                cmp = cmp.substr(0, pos);
            }
            if (NStr::Equal(cmp, strain)) {
                found = true;
                break;
            }
        }
    }
    return found;
}


bool HasStrainForATCCCultureCollection(const COrgName::TMod& mods, const string& culture_collection)
{
    if (NStr::IsBlank(culture_collection)) {
        return true;
    }
    bool found = false;
    for (const auto& m : mods) {
        if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_strain && m->IsSetSubname() && NStr::StartsWith(m->GetSubname(), "ATCC ")) {
            string cmp = m->GetSubname().substr(5);
            NStr::TruncateSpacesInPlace(cmp);
            size_t pos = NStr::Find(cmp, ";");
            if (pos != string::npos) {
                cmp = cmp.substr(0, pos);
            }
            if (NStr::Equal(cmp, culture_collection)) {
                found = true;
                break;
            }
        }
    }
    return found;
}


DISCREPANCY_CASE(ATCC_CULTURE_CONFLICT, BIOSRC, eDisc | eOncaller, "ATCC strain should also appear in culture collection")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            bool report = false;
            for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (m->IsSetSubtype() && m->IsSetSubname()) {
                    if (m->GetSubtype() == COrgMod::eSubtype_strain && NStr::StartsWith(m->GetSubname(), "ATCC ") && !HasCultureCollectionForATCCStrain(biosrc->GetOrg().GetOrgname().GetMod(), m->GetSubname().substr(5))) {
                        report = true;
                        break;
                    }
                    else if (m->GetSubtype() == COrgMod::eSubtype_culture_collection && NStr::StartsWith(m->GetSubname(), "ATCC:") && !HasStrainForATCCCultureCollection(biosrc->GetOrg().GetOrgname().GetMod(), m->GetSubname().substr(5))) {
                        report = true;
                        break;
                    }
                }
            }
            if (report) {
                m_Objs["[n] biosource[s] [has] conflicting ATCC strain and culture collection values"].Add(*context.BiosourceObjRef(*biosrc, true));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(ATCC_CULTURE_CONFLICT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool SetCultureCollectionFromStrain(CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetOrgMod() || !src.GetOrg().GetOrgname().IsSetMod()) {
        return false;
    }
    vector<string> add;
    for (const auto& m : src.GetOrg().GetOrgname().GetMod()) {
        if (m->IsSetSubtype() && m->IsSetSubname()) {
            if (m->GetSubtype() == COrgMod::eSubtype_strain && NStr::StartsWith(m->GetSubname(), "ATCC ") &&
                !HasCultureCollectionForATCCStrain(src.GetOrg().GetOrgname().GetMod(), m->GetSubname().substr(5))) {
                add.push_back("ATCC:" + m->GetSubname());
            }
        }
    }
    if (!add.empty()) {
        for (const string& s : add) {
            CRef<COrgMod> m(new COrgMod(COrgMod::eSubtype_culture_collection, s));
            src.SetOrg().SetOrgname().SetMod().push_back(m);
        }
        return true;
    }
    return false;
}


DISCREPANCY_AUTOFIX(ATCC_CULTURE_CONFLICT)
{
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    if (feat) {
        if (SetCultureCollectionFromStrain(const_cast<CSeq_feat*>(feat)->SetData().SetBiosrc())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("ATCC_CULTURE_CONFLICT: Set culture collection for [n] source[s]", 1));
        }
    }
    if (desc) {
        if (SetCultureCollectionFromStrain(const_cast<CSeqdesc*>(desc)->SetSource())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("ATCC_CULTURE_CONFLICT: Set culture collection for [n] source[s]", 1));
        }
    }
    return CRef<CAutofixReport>(0);
}


// BACTERIA_SHOULD_NOT_HAVE_ISOLATE

const string kAmplifiedWithSpeciesSpecificPrimers = "amplified with species-specific primers";

DISCREPANCY_CASE(BACTERIA_SHOULD_NOT_HAVE_ISOLATE, BIOSRC, eDisc | eOncaller | eSmart, "Bacterial sources should not have isolate")
{
    const CSeqdesc* src = context.GetBiosource();
    if (context.HasLineage(src ? &src->GetSource() : 0, "Bacteria") || context.HasLineage(src ? &src->GetSource() : 0, "Archaea")) {
        for (const CBioSource* biosrc : context.GetBiosources()) {
            bool has_bad_isolate = false;
            bool is_metagenomic = false;
            bool is_env_sample = false;
            if (biosrc->IsSetSubtype()) {
                for (const auto& s : biosrc->GetSubtype()) {
                    if (s->IsSetSubtype()) {
                        if (s->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                            is_env_sample = true;
                            if (is_metagenomic && is_env_sample) {
                                return;
                            }
                        }
                        if (s->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                            is_metagenomic = true;
                            if (is_metagenomic && is_env_sample) {
                                return;
                            }
                        }
                        if (s->GetSubtype() == CSubSource::eSubtype_other && s->IsSetName() && NStr::Equal(s->GetName(), kAmplifiedWithSpeciesSpecificPrimers)) {
                            return;
                        }
                    }
                }
            }
            if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (m->IsSetSubtype()) {
                        if (m->GetSubtype() == CSubSource::eSubtype_other && m->IsSetSubname() && NStr::Equal(m->GetSubname(), kAmplifiedWithSpeciesSpecificPrimers)) {
                            return;
                        }
                        if (m->GetSubtype() == COrgMod::eSubtype_isolate && m->IsSetSubname() &&
                            !NStr::StartsWith(m->GetSubname(), "DGGE gel band") &&
                            !NStr::StartsWith(m->GetSubname(), "TGGE gel band") &&
                            !NStr::StartsWith(m->GetSubname(), "SSCP gel band")) {
                            has_bad_isolate = true;
                        }
                    }
                }
            }
            if (has_bad_isolate) {
                m_Objs["[n] bacterial biosource[s] [has] isolate"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BACTERIA_SHOULD_NOT_HAVE_ISOLATE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MAG_SHOULD_NOT_HAVE_STRAIN

DISCREPANCY_CASE(MAG_SHOULD_NOT_HAVE_STRAIN, BIOSRC, eDisc | eSmart, "Organism assembled from metagenome reads should not have strain")
{
    const CSeqdesc* src = context.GetBiosource();
    if (context.HasLineage(src ? &src->GetSource() : 0, "Bacteria") || context.HasLineage(src ? &src->GetSource() : 0, "Archaea")) {
        for (const CBioSource* biosrc : context.GetBiosources()) {
            bool is_metagenomic = false;
            bool is_env_sample = false;
            if (biosrc->IsSetSubtype()) {
                for (const auto& s : biosrc->GetSubtype()) {
                    if (s->IsSetSubtype()) {
                        if (s->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                            is_env_sample = true;
                        }
                        if (s->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                            is_metagenomic = true;
                        }
                    }
                }
            }
            if (is_metagenomic && is_env_sample && biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_strain) {
                        m_Objs["[n] organism[s] assembled from metagenome [has] strain"].Add(*context.BiosourceObjRef(*biosrc));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MAG_SHOULD_NOT_HAVE_STRAIN)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MAG_MISSING_ISOLATE

DISCREPANCY_CASE(MAG_MISSING_ISOLATE, BIOSRC, eDisc | eSmart, "Organism assembled from metagenome reads should have isolate")
{
    const CSeqdesc* src = context.GetBiosource();
    if (context.HasLineage(src ? &src->GetSource() : 0, "Bacteria") || context.HasLineage(src ? &src->GetSource() : 0, "Archaea")) {
        for (const CBioSource* biosrc : context.GetBiosources()) {
            bool is_metagenomic = false;
            bool is_env_sample = false;
            bool has_isolate = false;
            if (biosrc->IsSetSubtype()) {
                for (const auto& s : biosrc->GetSubtype()) {
                    if (s->IsSetSubtype()) {
                        if (s->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                            is_env_sample = true;
                        }
                        if (s->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                            is_metagenomic = true;
                        }
                    }
                }
            }
            if (!is_metagenomic || !is_env_sample) {
                continue;
            }
            if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_isolate) {
                        has_isolate = true;
                        break;
                    }
                }
            }
            if (!has_isolate) {
                m_Objs["[n] organism[s] assembled from metagenome [is] missing isolate"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MAG_MISSING_ISOLATE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MULTISRC

DISCREPANCY_CASE(MULTISRC, BIOSRC, eDisc | eOncaller, "Comma or semicolon appears in strain or isolate")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            bool report = false;
            for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (m->IsSetSubtype() && (m->GetSubtype() == COrgMod::eSubtype_isolate || m->GetSubtype() == COrgMod::eSubtype_strain) && m->IsSetSubname() && (NStr::Find(m->GetSubname(), ",") != string::npos || NStr::Find(m->GetSubname(), ";") != string::npos)) {
                    report = true;
                    break;
                }
            }
            if (report) {
                m_Objs["[n] organism[s] [has] comma or semicolon in strain or isolate"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MULTISRC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MULTIPLE_CULTURE_COLLECTION

DISCREPANCY_CASE(MULTIPLE_CULTURE_COLLECTION, BIOSRC, eOncaller, "Multiple culture-collection quals")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            bool found = false;
            for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_culture_collection) {
                    if (found) {
                        m_Objs["[n] organism[s] [has] multiple culture-collection qualifiers"].Add(*context.BiosourceObjRef(*biosrc));
                        break;
                    }
                    found = true;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MULTIPLE_CULTURE_COLLECTION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// REQUIRED_STRAIN

DISCREPANCY_CASE(REQUIRED_STRAIN, BIOSRC, eDisc | eSubmitter | eSmart, "Bacteria should have strain")
{
    const CSeqdesc* src = context.GetBiosource();
    if (context.HasLineage(src ? &src->GetSource() : 0, "Bacteria") || context.HasLineage(src ? &src->GetSource() : 0, "Archaea")) {
        for (const CBioSource* biosrc : context.GetBiosources()) {
            if (biosrc->IsSetSubtype()) {
                bool is_metagenomic = false;
                bool is_env_sample = false;
                for (const auto& s : biosrc->GetSubtype()) {
                    if (s->IsSetSubtype()) {
                        if (s->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                            is_env_sample = true;
                            if (is_metagenomic) {
                                break;
                            }
                        }
                        if (s->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                            is_metagenomic = true;
                            if (is_env_sample) {
                                break;
                            }
                        }
                    }
                }
                if (is_metagenomic && is_env_sample) {
                    continue;
                }
            }
            if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                bool skip = false;
                for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_strain) {
                        skip = true;
                        break;
                    }
                }
                if (skip) {
                    continue;
                }
            }
            m_Objs["[n] biosource[s] [is] missing required strain value"].Add(*context.BiosourceObjRef(*biosrc));
        }
    }
}


DISCREPANCY_SUMMARIZE(REQUIRED_STRAIN)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// STRAIN_CULTURE_COLLECTION_MISMATCH

static bool MatchExceptSpaceColon(const string& a, const string& b)
{
    size_t i = 0;
    size_t j = 0;
    while (i < a.length() && j < b.length()) {
        while (i < a.length() && (a[i] == ':' || a[i] == ' ')) i++;
        while (j < b.length() && (b[j] == ':' || b[j] == ' ')) j++;
        if (i == a.length()) {
            return j == b.length();
        }
        if (j == b.length() || a[i] != b[j]) {
            return false;
        }
        i++;
        j++;
    }
    return true;
}


DISCREPANCY_CASE(STRAIN_CULTURE_COLLECTION_MISMATCH, BIOSRC, eOncaller | eSmart, "Strain and culture-collection values conflict")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            vector<const COrgMod*> OrgMods;
            for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                OrgMods.push_back(&*m);
            }
            bool match = false;
            bool conflict = false;
            for (size_t i = 0; i < OrgMods.size(); i++) {
                if (OrgMods[i]->IsSetSubtype() && OrgMods[i]->GetSubtype() == COrgMod::eSubtype_strain) {
                    for (size_t j = i + 1; j < OrgMods.size(); j++) { // not from 0 ?
                        if (OrgMods[j]->IsSetSubtype() && OrgMods[j]->GetSubtype() == COrgMod::eSubtype_culture_collection) {
                            if (MatchExceptSpaceColon(OrgMods[i]->GetSubname(), OrgMods[j]->GetSubname())) {
                                match = true;
                                break;
                            }
                            else {
                                conflict = true;
                            }
                        }
                    }
                    if (match) {
                        break;
                    }
                }
            }
            if (conflict && !match) {
                m_Objs["[n] organism[s] [has] conflicting strain and culture-collection values"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(STRAIN_CULTURE_COLLECTION_MISMATCH)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SP_NOT_UNCULTURED

DISCREPANCY_CASE(SP_NOT_UNCULTURED, BIOSRC, eOncaller, "Organism ending in sp. needs tax consult")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetTaxname()) {
            const string& s = biosrc->GetOrg().GetTaxname();
            if (s.length() > 4 && s.substr(s.length() - 4) == " sp." && s.substr(0, 11) != "uncultured ") {
                m_Objs["[n] biosource[s] [has] taxname[s] that end[S] with \' sp.\' but [does] not start with \'uncultured\'"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SP_NOT_UNCULTURED)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_STRAND_TRNAS

DISCREPANCY_CASE(FIND_STRAND_TRNAS, SEQUENCE, eDisc, "Find tRNAs on the same strand")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && biosrc->GetSource().IsSetGenome() && (biosrc->GetSource().GetGenome() == CBioSource::eGenome_mitochondrion || biosrc->GetSource().GetGenome() == CBioSource::eGenome_chloroplast || biosrc->GetSource().GetGenome() == CBioSource::eGenome_plastid)) {
        bool strand_plus = false;
        bool strand_minus = false;
        for (const auto& feat : context.FeatTRNAs()) {
            if (feat->GetLocation().GetStrand() == eNa_strand_minus) {
                strand_minus = true;
            }
            else {
                strand_plus = true;
            }
            if (strand_plus && strand_minus) {
                return;
            }
        }
        for (const auto& feat : context.FeatTRNAs()) {
            m_Objs[strand_plus ? "[n] tRNA[s] on plus strand" : "[n] tRNA[s] on minus strand"].Add(*context.SeqFeatObjRef(*feat));
        }
    }
}


DISCREPANCY_SUMMARIZE(FIND_STRAND_TRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// REQUIRED_CLONE

bool HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& src)
{
    if (src.IsSetSubtype()) {
        for (const auto& s : src.GetSubtype()) {
            if (s->IsSetSubtype() && s->GetSubtype() == CSubSource::eSubtype_other && s->IsSetName() && NStr::Equal(s->GetName(), kAmplifiedWithSpeciesSpecificPrimers)) {
                return true;
            }
        }
    }
    if (src.IsSetOrg() && src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetMod()) {
        for (const auto& m : src.GetOrg().GetOrgname().GetMod()) {
            if (m->IsSetSubtype() && m->GetSubtype() == CSubSource::eSubtype_other && m->IsSetSubname() && NStr::Equal(m->GetSubname(), kAmplifiedWithSpeciesSpecificPrimers)) {
                return true;
            }
        }
    }

    return false;
}


static bool IsMissingRequiredClone(const CBioSource& biosource)
{
    if (HasAmplifiedWithSpeciesSpecificPrimerNote(biosource)) {
        return false;
    }
    bool needs_clone = biosource.IsSetOrg() && biosource.GetOrg().IsSetTaxname() && NStr::StartsWith(biosource.GetOrg().GetTaxname(), "uncultured", NStr::eNocase);
    bool has_clone = false;
    if (biosource.IsSetSubtype()) {
        for (const auto& subtype_it : biosource.GetSubtype()) {
            if (subtype_it->IsSetSubtype()) {
                CSubSource::TSubtype subtype = subtype_it->GetSubtype();
                if (subtype == CSubSource::eSubtype_environmental_sample) {
                    needs_clone = true;
                }
                else if (subtype == CSubSource::eSubtype_clone) {
                    has_clone = true;
                }
            }
        }
    }
    if (needs_clone && !has_clone) {
        // look for gel band isolate
        bool has_gel_band_isolate = false;
        if (biosource.IsSetOrg() && biosource.GetOrg().IsSetOrgname() && biosource.GetOrg().GetOrgname().IsSetMod()) {
            for (const auto& mod_it : biosource.GetOrg().GetOrgname().GetMod()) {
                if (mod_it->IsSetSubtype() && mod_it->GetSubtype() == COrgMod::eSubtype_isolate) {
                    if (mod_it->IsSetSubname() && NStr::FindNoCase(mod_it->GetSubname(), "gel band") != NPOS) {
                        has_gel_band_isolate = true;
                        break;
                    }
                }
            }
        }
        if (has_gel_band_isolate) {
            needs_clone = false;
        }
    }
    return (needs_clone && !has_clone);
}


DISCREPANCY_CASE(REQUIRED_CLONE, BIOSRC, eOncaller, "Uncultured or environmental sources should have clone")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (IsMissingRequiredClone(*biosrc)) {
            m_Objs["[n] biosource[s] [is] missing required clone value"].Add(*context.BiosourceObjRef(*biosrc));
        }
    }
}


DISCREPANCY_SUMMARIZE(REQUIRED_CLONE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// STRAIN_TAXNAME_MISMATCH

DISCREPANCY_CASE(STRAIN_TAXNAME_MISMATCH, BIOSRC, eDisc | eOncaller, "BioSources with the same strain should have the same taxname")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            for (const auto& om : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (om->IsSetSubtype() && om->GetSubtype() == COrgMod::eSubtype_strain && om->IsSetSubname()) {
                    const string strain = om->GetSubname();
                    if (!strain.empty()) {
                        m_Objs[strain][biosrc->GetOrg().IsSetTaxname() ? biosrc->GetOrg().GetTaxname() : ""].Add(*context.BiosourceObjRef(*biosrc));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(STRAIN_TAXNAME_MISMATCH)
{
    CReportNode rep, rep1;
    static const string root = "[n] biosources have strain/taxname conflicts";
    for (auto& it: m_Objs.GetMap()) {
        if (it.second->GetMap().size() > 1) {
            for (auto& mm: it.second->GetMap()) {
                for (auto& obj : mm.second->GetObjects()) {
                    string label = "[n] biosources have strain [(]" + it.first + "[)] but do not have the same taxnames";
                    rep["[n] biosources have strain/taxname conflicts"][label].Ext().Add(*obj);
                    rep1[label].Add(*obj);
                }
            }
        }
    }
    m_ReportItems = rep1.GetMap().size() > 1 ? rep.Export(*this)->GetSubitems() : rep1.Export(*this)->GetSubitems();
}


// SPECVOUCHER_TAXNAME_MISMATCH

DISCREPANCY_CASE(SPECVOUCHER_TAXNAME_MISMATCH, BIOSRC, eOncaller | eSmart, "BioSources with the same specimen voucher should have the same taxname")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            for (const auto& om : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (om->IsSetSubtype() && om->GetSubtype() == COrgMod::eSubtype_specimen_voucher && om->IsSetSubname()) {
                    const string strain = om->GetSubname();
                    if (!strain.empty()) {
                        m_Objs[strain][biosrc->GetOrg().IsSetTaxname() ? biosrc->GetOrg().GetTaxname() : ""].Add(*context.BiosourceObjRef(*biosrc));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SPECVOUCHER_TAXNAME_MISMATCH)
{
    CReportNode rep, rep1;
    for (auto& it: m_Objs.GetMap()) {
        if (it.second->GetMap().size() > 1) {
            for (auto& mm: it.second->GetMap()) {
                for (auto& obj: mm.second->GetObjects()) {
                    string label = "[n] biosources have specimen voucher [(]" + it.first + "[)] but do not have the same taxnames";
                    rep["[n] biosources have specimen voucher/taxname conflicts"][label].Ext().Add(*obj);
                    rep1[label].Add(*obj);
                }
            }
        }
    }
    m_ReportItems = rep1.GetMap().size() > 1 ? rep.Export(*this)->GetSubitems() : rep1.Export(*this)->GetSubitems();
}


// CULTURE_TAXNAME_MISMATCH

DISCREPANCY_CASE(CULTURE_TAXNAME_MISMATCH, BIOSRC, eOncaller, "Test BioSources with the same culture collection but different taxname")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            for (const auto& om : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (om->IsSetSubtype() && om->GetSubtype() == COrgMod::eSubtype_culture_collection && om->IsSetSubname()) {
                    const string strain = om->GetSubname();
                    if (!strain.empty()) {
                        m_Objs[strain][biosrc->GetOrg().IsSetTaxname() ? biosrc->GetOrg().GetTaxname() : ""].Add(*context.BiosourceObjRef(*biosrc));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CULTURE_TAXNAME_MISMATCH)
{
    CReportNode rep, rep1;
    for (auto& it : m_Objs.GetMap()) {
        if (it.second->GetMap().size() > 1) {
            for (auto& mm : it.second->GetMap()) {
                for (auto& obj : mm.second->GetObjects()) {
                    string label = "[n] biosources have culture collection " + it.first + " but do not have the same taxnames";
                    rep["[n] biosources have culture collection/taxname conflicts"][label].Ext().Add(*obj);
                    rep1[label].Add(*obj);
                }
            }
        }
    }
    m_ReportItems = rep1.GetMap().size() > 1 ? rep.Export(*this)->GetSubitems() : rep1.Export(*this)->GetSubitems();
}


// BIOMATERIAL_TAXNAME_MISMATCH

DISCREPANCY_CASE(BIOMATERIAL_TAXNAME_MISMATCH, BIOSRC, eOncaller | eSmart, "Test BioSources with the same biomaterial but different taxname")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
            for (const auto& om : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (om->IsSetSubtype() && om->GetSubtype() == COrgMod::eSubtype_bio_material && om->IsSetSubname()) {
                    const string strain = om->GetSubname();
                    if (!strain.empty()) {
                        m_Objs[strain][biosrc->GetOrg().IsSetTaxname() ? biosrc->GetOrg().GetTaxname() : ""].Add(*context.BiosourceObjRef(*biosrc));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BIOMATERIAL_TAXNAME_MISMATCH)
{
    {
        CReportNode rep, rep1;
        for (auto& it : m_Objs.GetMap()) {
            if (it.second->GetMap().size() > 1) {
                for (auto& mm : it.second->GetMap()) {
                    for (auto& obj : mm.second->GetObjects()) {
                        string label = "[n] biosources have biomaterial " + it.first + " but do not have the same taxnames";
                        rep["[n] biosources have biomaterial/taxname conflicts"][label].Ext().Add(*obj);
                        rep1[label].Add(*obj);
                    }
                }
            }
        }
        m_ReportItems = rep1.GetMap().size() > 1 ? rep.Export(*this)->GetSubitems() : rep1.Export(*this)->GetSubitems();
    }
}


// ORGANELLE_ITS

DISCREPANCY_CASE(ORGANELLE_ITS, SEQUENCE, eOncaller, "Test Bioseqs for suspect rRNA / ITS on organelle")
{
    static const vector<string> suspectable_products = {
        "18S ribosomal RNA",
        "5.8S ribosomal RNA",
        "25S ribosomal RNA",
        "28S ribosomal RNA",
        "internal transcribed spacer 1",
        "internal transcribed spacer 2"
    };
    static const string msg = "[n] Bioseq[s] [has] suspect rRNA / ITS on organelle";
    const CSeqdesc* src = context.GetBiosource();
    if (src && src->GetSource().IsSetGenome()) {
        int genome = src->GetSource().GetGenome();
        if (genome == CBioSource::eGenome_apicoplast || genome == CBioSource::eGenome_chloroplast || genome == CBioSource::eGenome_chromatophore
                || genome == CBioSource::eGenome_chromoplast || genome == CBioSource::eGenome_cyanelle || genome == CBioSource::eGenome_hydrogenosome
                || genome == CBioSource::eGenome_kinetoplast || genome == CBioSource::eGenome_leucoplast || genome == CBioSource::eGenome_mitochondrion
                || genome == CBioSource::eGenome_plastid || genome == CBioSource::eGenome_proplastid) {
            for (const CSeq_feat& feat : context.GetFeat()) {
                if (feat.IsSetData() && feat.GetData().IsRna()) {
                    const CRNA_ref& rna = feat.GetData().GetRna();
                    if (rna.IsSetType() && (rna.GetType() == CRNA_ref::eType_rRNA || rna.GetType() == CRNA_ref::eType_miscRNA)) {
                        const string& product = rna.GetRnaProductName();
                        // The Owls Are Not What They Seem!
                        // if (NStr::FindNoCase(suspectable_products, product) != nullptr) {
                        if (!product.empty()) {
                            for (const string& pattern : suspectable_products) {
                                if (NStr::FindNoCase(product, pattern) != NPOS) {
                                    m_Objs[msg].Add(*context.BioseqObjRef());
                                    return;
                                }
                            }
                        }
                        if (feat.IsSetComment()) {
                            const string& comment = feat.GetComment();
                            // The Owls Are Not What They Seem!
                            // if (!comment.empty() && NStr::FindNoCase(suspectable_products, comment) != nullptr) {
                            if (!comment.empty()) {
                                for (const string& pattern : suspectable_products) {
                                    if (NStr::FindNoCase(comment, pattern) != NPOS) {
                                        m_Objs[msg].Add(*context.BioseqObjRef());
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(ORGANELLE_ITS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INCONSISTENT_BIOSOURCE

typedef list<string> TInconsistecyDescriptionList;

template<class T, typename R> class CCompareValues
{
    typedef bool (T::*TIsSetFn)() const;
    typedef int (T::*TGetIntFn)() const;
    typedef const R& (T::*TGetRFn)() const;

public:
    static bool IsEqualInt(const T& first, const T& second, TIsSetFn is_set_fn, TGetIntFn get_fn, int not_set)
    {
        int first_val = (first.*is_set_fn)() ? (first.*get_fn)() : not_set,
            second_val = (second.*is_set_fn)() ? (second.*get_fn)() : not_set;

        return first_val == second_val;
    }

    static bool IsEqualVal(const T& first, const T& second, TIsSetFn is_set_fn, TGetRFn get_fn, const R& empty_val)
    {
        const R& first_val = (first.*is_set_fn)() ? (first.*get_fn)() : empty_val,
               & second_val = (second.*is_set_fn)() ? (second.*get_fn)() : empty_val;

        return first_val == second_val;
    }
};

static bool IsSameSubtype(const CBioSource::TSubtype& first, const CBioSource::TSubtype& second)
{
    if (first.size() != second.size()) {
        return false;
    }

    for (CBioSource::TSubtype::const_iterator it_first = first.cbegin(), it_second = second.cbegin();
         it_first != first.cend();
         ++it_first, ++it_second) {

        if (!CCompareValues<CSubSource, int>::IsEqualInt(**it_first, **it_second, &CSubSource::IsSetSubtype, &CSubSource::GetSubtype, 0)) {
            return false;
        }

        if (!CCompareValues<CSubSource, string>::IsEqualVal(**it_first, **it_second, &CSubSource::IsSetName, &CSubSource::GetName, "")) {
            return false;
        }

        if (!CCompareValues<CSubSource, string>::IsEqualVal(**it_first, **it_second, &CSubSource::IsSetAttrib, &CSubSource::GetAttrib, "")) {
            return false;
        }
    }

    return true;
}

static bool IsSameDb(const COrg_ref::TDb& first, const COrg_ref::TDb& second)
{
    if (first.size() != second.size()) {
        return false;
    }

    for (COrg_ref::TDb::const_iterator it_first = first.cbegin(), it_second = second.cbegin();
         it_first != first.cend();
         ++it_first, ++it_second) {

        if (!(*it_first)->Equals(**it_second)) {
            return false;
        }
    }

    return true;
}

static void GetOrgnameDifferences(const COrgName& first, const COrgName& second, TInconsistecyDescriptionList& diffs)
{
    bool first_name_set = first.IsSetName(),
         second_name_set = second.IsSetName();

    if (first_name_set != second_name_set || (first_name_set && first.GetName().Which() != second.GetName().Which())) {
        diffs.push_back("orgname choices differ");
    }

    if (!CCompareValues<COrgName, int>::IsEqualInt(first, second, &COrgName::IsSetGcode, &COrgName::GetGcode, -1)) {
        diffs.push_back("genetic codes differ");
    }

    if (!CCompareValues<COrgName, int>::IsEqualInt(first, second, &COrgName::IsSetMgcode, &COrgName::GetMgcode, -1)) {
        if (first.IsSetMgcode() && first.GetMgcode() || second.IsSetMgcode() && second.GetMgcode()) {
            diffs.push_back("mitochondrial genetic codes differ");
        }
    }

    if (!CCompareValues<COrgName, string>::IsEqualVal(first, second, &COrgName::IsSetAttrib, &COrgName::GetAttrib, "")) {
        diffs.push_back("attributes differ");
    }

    if (!CCompareValues<COrgName, string>::IsEqualVal(first, second, &COrgName::IsSetLineage, &COrgName::GetLineage, "")) {
        diffs.push_back("lineages differ");
    }

    if (!CCompareValues<COrgName, string>::IsEqualVal(first, second, &COrgName::IsSetDiv, &COrgName::GetDiv, "")) {
        diffs.push_back("divisions differ");
    }

    bool first_mod_set = first.IsSetMod(),
         second_mod_set = second.IsSetMod();

    COrgName::TMod::const_iterator it_first, it_second;
    if (first_mod_set) {
        it_first = first.GetMod().cbegin();
    }
    if (second_mod_set) {
        it_second = second.GetMod().cbegin();
    }
    if (first_mod_set && second_mod_set) {
        COrgName::TMod::const_iterator end_first = first.GetMod().cend(),
            end_second = second.GetMod().cend();

        for (; it_first != end_first && it_second != end_second; ++it_first, ++it_second) {

            const string& qual = (*it_first)->IsSetSubtype() ? COrgMod::ENUM_METHOD_NAME(ESubtype)()->FindName((*it_first)->GetSubtype(), true) : "Unknown source qualifier";

            if (!CCompareValues<COrgMod, int>::IsEqualInt(**it_first, **it_second, &COrgMod::IsSetSubtype, &COrgMod::GetSubtype, -1)) {
                diffs.push_back("missing " + qual + " modifier");
            }

            if (!CCompareValues<COrgMod, string>::IsEqualVal(**it_first, **it_second, &COrgMod::IsSetAttrib, &COrgMod::GetAttrib, "")) {
                diffs.push_back(qual + " modifier attrib values differ");
            }

            if (!CCompareValues<COrgMod, string>::IsEqualVal(**it_first, **it_second, &COrgMod::IsSetSubname, &COrgMod::GetSubname, "")) {
                diffs.push_back("different " + qual + " values");
            }
        }

        if (it_first == end_first) {
            first_mod_set = false;
        }
        if (it_second == end_second) {
            second_mod_set = false;
        }
    }

    if (first_mod_set && !second_mod_set) {
        const string& qual = (*it_first)->IsSetSubtype() ? ENUM_METHOD_NAME(ESource_qual)()->FindName((*it_first)->GetSubtype(), true) : "Unknown source qualifier";
        diffs.push_back("missing " + qual + " modifier");
    }
    else if (!first_mod_set && second_mod_set) {
        const string& qual = (*it_second)->IsSetSubtype() ? ENUM_METHOD_NAME(ESource_qual)()->FindName((*it_second)->GetSubtype(), true) : "Unknown source qualifier";
        diffs.push_back("missing " + qual + " modifier");
    }
}

static void GetOrgrefDifferences(const COrg_ref& first_org, const COrg_ref& second_org, TInconsistecyDescriptionList& diffs)
{
    if (!CCompareValues<COrg_ref, string>::IsEqualVal(first_org, second_org, &COrg_ref::IsSetTaxname, &COrg_ref::GetTaxname, "")) {
        diffs.push_back("taxnames differ");
    }

    if (!CCompareValues<COrg_ref, string>::IsEqualVal(first_org, second_org, &COrg_ref::IsSetCommon, &COrg_ref::GetCommon, "")) {
        diffs.push_back("common names differ");
    }

    if (!CCompareValues<COrg_ref, COrg_ref::TSyn>::IsEqualVal(first_org, second_org, &COrg_ref::IsSetSyn, &COrg_ref::GetSyn, COrg_ref::TSyn())) {
        diffs.push_back("synonyms differ");
    }

    bool first_db_set = first_org.IsSetDb(),
         second_db_set = second_org.IsSetDb();

    if (first_db_set != second_db_set || (first_db_set && !IsSameDb(first_org.GetDb(), second_org.GetDb()))) {
        diffs.push_back("dbxrefs differ");
    }

    bool first_orgname_set = first_org.IsSetOrgname(),
         second_orgname_set = second_org.IsSetOrgname();

    if (first_orgname_set != second_orgname_set) {
        diffs.push_back("one Orgname is missing");
    }
    else if (first_orgname_set && second_orgname_set) {
        GetOrgnameDifferences(first_org.GetOrgname(), second_org.GetOrgname(), diffs);
    }
}


static void GetBiosourceDifferences(const CBioSource& first_biosrc, const CBioSource& second_biosrc, TInconsistecyDescriptionList& diffs)
{
    if (!CCompareValues<CBioSource, int>::IsEqualInt(first_biosrc, second_biosrc, &CBioSource::IsSetOrigin, &CBioSource::GetOrigin, CBioSource::eOrigin_unknown)) {
        diffs.push_back("origins differ");
    }

    if (first_biosrc.IsSetIs_focus() != second_biosrc.IsSetIs_focus()) {
        diffs.push_back("focus differs");
    }

    if (!CCompareValues<CBioSource, int>::IsEqualInt(first_biosrc, second_biosrc, &CBioSource::IsSetGenome, &CBioSource::GetGenome, CBioSource::eGenome_unknown)) {
        diffs.push_back("locations differ");
    }

    static const CBioSource::TSubtype empty_subtype;

    const CBioSource::TSubtype& first_subtype = first_biosrc.IsSetSubtype() ? first_biosrc.GetSubtype() : empty_subtype,
                              & second_subtype = second_biosrc.IsSetSubtype() ? second_biosrc.GetSubtype() : empty_subtype;
    if (!IsSameSubtype(first_subtype, second_subtype)) {
        diffs.push_back("subsource qualifiers differ");
    }

    bool first_org_set = first_biosrc.IsSetOrg(),
         second_org_set = second_biosrc.IsSetOrg();

    if (first_org_set != second_org_set) {
        diffs.push_back("one OrgRef is missing");
    }
    else if (first_org_set && second_org_set) {
        GetOrgrefDifferences(first_biosrc.GetOrg(), second_biosrc.GetOrg(), diffs);
    }
}


DISCREPANCY_CASE(INCONSISTENT_BIOSOURCE, SEQUENCE, eDisc | eSubmitter | eSmart, "Inconsistent BioSource")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsNa()) {
        const CSeqdesc* biosrc = context.GetBiosource();
        if (biosrc) {
            stringstream ss;
            ss << MSerial_AsnBinary << biosrc->GetSource();
            auto& node = m_Objs[ss.str()];
            node.Add(*context.SeqdescObjRef(*biosrc));
            node.Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_BIOSOURCE)
{
    auto& M = m_Objs.GetMap();
    string subtype;
    for (auto a = M.cbegin(); a != M.cend(); ++a) {
        stringstream ss(a->first);
        CBioSource bs_a;
        ss >> MSerial_AsnBinary >> bs_a;
        auto b = a;
        for (++b; b != M.cend(); ++b) {
            stringstream ss(b->first);
            CBioSource bs_b;
            ss >> MSerial_AsnBinary >> bs_b;
            TInconsistecyDescriptionList diffs;
            GetBiosourceDifferences(bs_a, bs_b, diffs);
            if (!diffs.empty()) {
                subtype = "[n/2] inconsistent contig source[s][(] (" + NStr::Join(diffs, ", ") + ")";
                break;
            }
        }
        if (!subtype.empty()) {
            break;
        }
    }
    if (!subtype.empty()) {
        CReportNode rep;
        size_t subcat_index = 0;
        static size_t MAX_NUM_LEN = 10;
        for (auto& it : M) {
            string subcat_num = NStr::SizetToString(subcat_index);
            subcat_num = string(MAX_NUM_LEN - subcat_num.size(), '0') + subcat_num;
            string subcat = "[*" + subcat_num + "*][n/2] contig[s] [has] identical sources that do not match another contig source";
            ++subcat_index;
            rep[subtype][subcat].Ext().Add(it.second->GetObjects());
        }
        m_ReportItems = rep.Export(*this)->GetSubitems();
    }
}


// TAX_LOOKUP_MISMATCH

DISCREPANCY_CASE(TAX_LOOKUP_MISMATCH, BIOSRC, eDisc, "Find Tax Lookup Mismatches")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg()) {
            stringstream ss;
            ss << MSerial_AsnBinary << biosrc->GetOrg();
            m_Objs[ss.str()].Add(*context.BiosourceObjRef(*biosrc));
        }
    }
}


static const CDbtag* GetTaxonTag(const COrg_ref& org)
{
    if (org.IsSetDb()) {
        for (const auto& db : org.GetDb()) {
            if (db->IsSetDb() && NStr::EqualNocase(db->GetDb(), "taxon")) {
                return db;
            }
        }
    }
    return 0;
}


static bool OrgDiffers(const COrg_ref& first, const COrg_ref& second)
{
    bool first_set = first.IsSetTaxname(), second_set = second.IsSetTaxname();
    if (first_set != second_set || (first_set && first.GetTaxname() != second.GetTaxname())) {
        return true;
    }
    const CDbtag* first_db_tag = GetTaxonTag(first);
    const CDbtag* second_db_tag = GetTaxonTag(second);
    if (first_db_tag == nullptr || second_db_tag == nullptr) {
        return true;
    }
    return !first_db_tag->Equals(*second_db_tag);
}


static CRef<CTaxon3_reply> GetOrgRefs(vector<CRef<COrg_ref>>& orgs)
{
    CTaxon3 taxon3;
    taxon3.Init();
    CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(orgs);
    return reply;
}


DISCREPANCY_SUMMARIZE(TAX_LOOKUP_MISMATCH)
{
    if (!m_Objs.empty()) {
        CReportNode rep;
        vector<CRef<COrg_ref>> org_refs;
        for (auto& it : m_Objs.GetMap()) {
            CRef<COrg_ref> oref(new COrg_ref());
            stringstream ss(it.first);
            ss >> MSerial_AsnBinary >> *oref;
            org_refs.push_back(oref);
        }
        CRef<CTaxon3_reply> reply = GetOrgRefs(org_refs);
        if (reply) {
            const auto& replies = reply->GetReply();
            auto rit = replies.cbegin();
            for (auto& it : m_Objs.GetMap()) {
                CRef<COrg_ref> oref(new COrg_ref());
                stringstream ss(it.first);
                ss >> MSerial_AsnBinary >> *oref;
                if ((*rit)->IsData() && OrgDiffers(*oref, (*rit)->GetData().GetOrg())) {
                    rep["[n] tax name[s] [does] not match taxonomy lookup"].Add(it.second->GetObjects());
                }
                ++rit;
            }
        }
        m_ReportItems = rep.Export(*this)->GetSubitems();
    }
}


// TAX_LOOKUP_MISSING

DISCREPANCY_CASE(TAX_LOOKUP_MISSING, BIOSRC, eDisc, "Find Missing Tax Lookup")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg()) {
            stringstream ss;
            ss << MSerial_AsnBinary << biosrc->GetOrg();
            m_Objs[ss.str()].Add(*context.BiosourceObjRef(*biosrc));
        }
    }
}


DISCREPANCY_SUMMARIZE(TAX_LOOKUP_MISSING)
{
    if (!m_Objs.empty()) {
        CReportNode rep;
        vector<CRef<COrg_ref>> org_refs;
        for (auto& it : m_Objs.GetMap()) {
            CRef<COrg_ref> oref(new COrg_ref());
            stringstream ss(it.first);
            ss >> MSerial_AsnBinary >> *oref;
            org_refs.push_back(oref);
        }
        CRef<CTaxon3_reply> reply = GetOrgRefs(org_refs);
        if (reply) {
            const auto& replies = reply->GetReply();
            auto rit = replies.cbegin();
            for (auto& it : m_Objs.GetMap()) {
                if (!(*rit)->IsData() || (*rit)->IsError()) {
                    rep["[n] tax name[s] [is] missing in taxonomy lookup"].Add(it.second->GetObjects());
                }
                ++rit;
            }
        }
        m_ReportItems = rep.Export(*this)->GetSubitems();
    }
}


// UNNECESSARY_ENVIRONMENTAL

DISCREPANCY_CASE(UNNECESSARY_ENVIRONMENTAL, BIOSRC, eOncaller, "Unnecessary environmental qualifier present")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetSubtype()) {
            bool skip = false;
            bool found = false;
            for (const auto& subtype : biosrc->GetSubtype()) {
                if (subtype->IsSetSubtype()) {
                    CSubSource::TSubtype st = subtype->GetSubtype();
                    if (st == CSubSource::eSubtype_metagenomic) {
                        skip = true;
                        break;
                    }
                    else if (st == CSubSource::eSubtype_other && NStr::FindNoCase(subtype->GetName(), "amplified with species-specific primers") != NPOS) {
                        skip = true;
                        break;
                    }
                    else if (st == CSubSource::eSubtype_environmental_sample) {
                        found = true;
                    }
                }
            }
            if (found && !skip) {
                if (biosrc->IsSetOrg()) {
                    if (biosrc->GetOrg().IsSetTaxname()) {
                        const string& s = biosrc->GetOrg().GetTaxname();
                        if (NStr::FindNoCase(s, "uncultured") != NPOS || NStr::FindNoCase(s, "enrichment culture") != NPOS || NStr::FindNoCase(s, "metagenome") != NPOS || NStr::FindNoCase(s, "environmental") != NPOS || NStr::FindNoCase(s, "unidentified") != NPOS) {
                            skip = true;
                            continue;
                        }
                    }
                    if (biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                        for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                            if (it->IsSetSubtype() && it->GetSubtype() == COrgMod::eSubtype_other && it->IsSetSubname() && NStr::FindNoCase(it->GetSubname(), "amplified with species-specific primers") != NPOS) {
                                skip = true;
                                break;
                            }
                        }
                    }
                }
                if (!skip) {
                    m_Objs["[n] biosource[s] [has] unnecessary environmental qualifier"].Add(*context.BiosourceObjRef(*biosrc));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNNECESSARY_ENVIRONMENTAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// END_COLON_IN_COUNTRY

DISCREPANCY_CASE(END_COLON_IN_COUNTRY, BIOSRC, eOncaller, "Country name end with colon")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetSubtype()) {
            for (const auto& subtype : biosrc->GetSubtype()) {
                if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_country) {
                    const string& s = subtype->GetName();
                    if (s.length() && s[s.length() - 1] == ':') {
                        m_Objs["[n] country source[s] end[S] with a colon."].Add(*context.BiosourceObjRef(*biosrc, true));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(END_COLON_IN_COUNTRY)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool RemoveCountryColon(CBioSource& src)
{
    if (!src.IsSetSubtype()) {
        return false;
    }
    bool fixed = false;
    for (const auto& subtype : src.GetSubtype()) {
        if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_country) {
            CSubSource& ss = const_cast<CSubSource&>(*subtype);
            string& s = ss.SetName();
            while (s.length() && s[s.length()-1] == ':') {
                s.resize(s.length()-1);
                fixed = true;
            }
        }
    }
    return fixed;
}


DISCREPANCY_AUTOFIX(END_COLON_IN_COUNTRY)
{
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    if (feat) {
        if (RemoveCountryColon(const_cast<CSeq_feat*>(feat)->SetData().SetBiosrc())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("END_COLON_IN_COUNTRY: [n] country name[s] fixed", 1));
        }
    }
    if (desc) {
        if (RemoveCountryColon(const_cast<CSeqdesc*>(desc)->SetSource())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("END_COLON_IN_COUNTRY: [n] country name[s] fixed", 1));
        }
    }
    return CRef<CAutofixReport>(0);
}


// COUNTRY_COLON

DISCREPANCY_CASE(COUNTRY_COLON, BIOSRC, eOncaller, "Country description should only have 1 colon")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetSubtype()) {
            for (const auto& subtype : biosrc->GetSubtype()) {
                if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_country) {
                    const string& s = subtype->GetName();
                    int count = 0;
                    for (size_t i = 0; i < s.length(); i++) {
                        if (s[i] == ':') {
                            count++;
                            if (count > 1) {
                                m_Objs["[n] country source[s] [has] more than 1 colon."].Add(*context.BiosourceObjRef(*biosrc, true));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(COUNTRY_COLON)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool ChangeCountryColonToComma(CBioSource& src)
{
    if (!src.IsSetSubtype()) {
        return false;
    }
    bool fixed = false;
    for (const auto& subtype : src.GetSubtype()) {
        if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_country) {
            CSubSource& ss = const_cast<CSubSource&>(*subtype);
            string& s = ss.SetName();
            int count = 0;
            for (size_t i = 0; i < s.length(); i++) {
                if (s[i] == ':') {
                    count++;
                    if (count > 1) {
                        s[i] = ',';
                        fixed = true;
                    }
                }
            }
        }
    }
    return fixed;
}


DISCREPANCY_AUTOFIX(COUNTRY_COLON)
{
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    if (feat) {
        if (ChangeCountryColonToComma(const_cast<CSeq_feat*>(feat)->SetData().SetBiosrc())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("COUNTRY_COLON: [n] country name[s] fixed", 1));
        }
    }
    if (desc) {
        if (ChangeCountryColonToComma(const_cast<CSeqdesc*>(desc)->SetSource())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("COUNTRY_COLON: [n] country name[s] fixed", 1));
        }
    }
    return CRef<CAutofixReport>(0);
}


// HUMAN_HOST

DISCREPANCY_CASE(HUMAN_HOST, BIOSRC, eDisc | eOncaller, "\'Human\' in host should be \'Homo sapiens\'")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetOrgname() && biosrc->GetOrg().GetOrgname().CanGetMod()) {
            for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (it->CanGetSubtype() && it->GetSubtype() == COrgMod::eSubtype_nat_host && NStr::FindNoCase(it->GetSubname(), "human") != NPOS) {
                    m_Objs["[n] organism[s] [has] \'human\' host qualifiers"].Add(*context.BiosourceObjRef(*biosrc, true));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(HUMAN_HOST)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool FixHumanHost(CBioSource& src)
{
    if (!src.IsSetOrg()) {
        return false;
    }
    bool fixed = false;
    for (const auto& it : src.GetOrg().GetOrgname().GetMod()) {
        if (it->CanGetSubtype() && it->GetSubtype() == COrgMod::eSubtype_nat_host && NStr::FindNoCase(it->GetSubname(), "human") != NPOS) {
            COrgMod& om = const_cast<COrgMod&>(*it);
            NStr::ReplaceInPlace(om.SetSubname(), "human", "Homo sapiens");
            fixed = true;
        }
    }
    return fixed;
}


DISCREPANCY_AUTOFIX(HUMAN_HOST)
{
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    if (feat) {
        if (FixHumanHost(const_cast<CSeq_feat*>(feat)->SetData().SetBiosrc())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("HUMAN_HOST: [n] host qualifier[s] fixed", 1));
        }
    }
    if (desc) {
        if (FixHumanHost(const_cast<CSeqdesc*>(desc)->SetSource())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("HUMAN_HOST: [n] host qualifier[s] fixed", 1));
        }
    }
    return CRef<CAutofixReport>(0);
}


// CHECK_AUTHORITY

DISCREPANCY_CASE(CHECK_AUTHORITY, BIOSRC, eDisc | eOncaller, "Authority and Taxname should match first two words")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetOrgname() && biosrc->GetOrg().GetOrgname().CanGetMod() && biosrc->GetOrg().CanGetTaxname() && biosrc->GetOrg().GetTaxname().length()) {
            string tax1, tax2;
            for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (it->CanGetSubtype() && it->GetSubtype() == COrgMod::eSubtype_authority) {
                    if (tax1.empty()) {
                        list<CTempString> tmp;
                        NStr::Split(biosrc->GetOrg().GetTaxname(), " ", tmp, NStr::fSplit_Tokenize);
                        list<CTempString>::const_iterator p = tmp.cbegin();
                        if (p != tmp.cend()) {
                            tax1 = *p;
                            p++;
                            if (p != tmp.cend()) {
                                tax2 = *p;
                            }
                        }
                    }
                    string aut1, aut2;
                    list<CTempString> tmp;
                    NStr::Split(it->GetSubname(), " ", tmp, NStr::fSplit_Tokenize);
                    list<CTempString>::const_iterator p = tmp.cbegin();
                    if (p != tmp.cend()) {
                        aut1 = *p;
                        p++;
                        if (p != tmp.cend()) {
                            aut2 = *p;
                        }
                    }
                    if (aut1 != tax1 || aut2 != tax2) {
                        m_Objs["[n] biosource[s] [has] taxname/authority conflict"].Add(*context.BiosourceObjRef(*biosrc));
                    }
                }
            }
        }
    }

}


DISCREPANCY_SUMMARIZE(CHECK_AUTHORITY)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// TRINOMIAL_SHOULD_HAVE_QUALIFIER

static const pair<int, string> srcqual_keywords[] = {
    { COrgMod::eSubtype_forma_specialis, " f. sp." } ,
    { COrgMod::eSubtype_forma, " f." } ,
    { COrgMod::eSubtype_sub_species, " subsp." } ,
    { COrgMod::eSubtype_variety, " var." } ,
    { COrgMod::eSubtype_pathovar, " pv." }
};

static const size_t srcqual_keywords_sz = sizeof(srcqual_keywords) / sizeof(srcqual_keywords[0]);

static string GetSrcQual(const CBioSource& bs, int qual)
{
    if (bs.GetOrg().CanGetOrgname() && bs.GetOrg().GetOrgname().CanGetMod()) {
        for (const auto& it : bs.GetOrg().GetOrgname().GetMod()) {
            if (it->CanGetSubtype() && it->GetSubtype() == qual) {
                return it->GetSubname();
            }
        }
    }
    return kEmptyStr;
}


DISCREPANCY_CASE(TRINOMIAL_SHOULD_HAVE_QUALIFIER, BIOSRC, eDisc | eOncaller | eSmart, "Trinomial sources should have corresponding qualifier")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetTaxname() && biosrc->GetOrg().GetTaxname().length() && NStr::FindNoCase(biosrc->GetOrg().GetTaxname(), " x ") == NPOS && !CDiscrepancyContext::HasLineage(*biosrc, context.GetLineage(), "Viruses")) {
            const string& taxname = biosrc->GetOrg().GetTaxname();
            for (size_t i = 0; i < srcqual_keywords_sz; i++) {
                size_t n = NStr::FindNoCase(taxname, srcqual_keywords[i].second);
                if (n != NPOS) {
                    for (n += srcqual_keywords[i].second.length(); n < taxname.length(); n++) {
                        if (taxname[n] != ' ') {
                            break;
                        }
                    }
                    if (n < taxname.length()) {
                        string q = GetSrcQual(*biosrc, srcqual_keywords[i].first);
                        string s = taxname.substr(n, q.length());
                        if (!q.length() || NStr::CompareNocase(s, q)) {
                            m_Objs["[n] trinomial source[s] lack[S] corresponding qualifier"].Add(*context.BiosourceObjRef(*biosrc));
                        }
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(TRINOMIAL_SHOULD_HAVE_QUALIFIER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE

DISCREPANCY_CASE(AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE, BIOSRC, eOncaller, "Species-specific primers, no environmental sample")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (!biosrc->HasSubtype(CSubSource::eSubtype_environmental_sample)) {
            bool has_primer_note = false;
            if (biosrc->CanGetSubtype()) {
                for (const auto& it : biosrc->GetSubtype()) {
                    if (it->GetSubtype() == CSubSource::eSubtype_other && NStr::FindNoCase(it->GetName(), "amplified with species-specific primers") != NPOS) {
                        has_primer_note = true;
                        break;
                    }
                }
            }
            if (!has_primer_note && biosrc->IsSetOrg() && biosrc->GetOrg().CanGetOrgname() && biosrc->GetOrg().GetOrgname().CanGetMod()) {
                for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                    if (it->CanGetSubtype() && it->GetSubtype() == COrgMod::eSubtype_other && it->IsSetSubname() && NStr::FindNoCase(it->GetSubname(), "amplified with species-specific primers") != NPOS) {
                        has_primer_note = true;
                        break;
                    }
                }
            }
            if (has_primer_note) {
                m_Objs["[n] biosource[s] [has] \'amplified with species-specific primers\' note but no environmental-sample qualifier."].Add(*context.BiosourceObjRef(*biosrc, true));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool SetEnvSampleFixAmplifiedPrimers(CBioSource& src)
{
    bool change = false;
    if (!src.HasSubtype(CSubSource::eSubtype_environmental_sample)) {
        src.SetSubtype().push_back(CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_environmental_sample, " ")));
        change = true;
    }
    for (auto& s : src.SetSubtype()) {
        if (s->GetSubtype() == CSubSource::eSubtype_other && s->IsSetName()) {
            const string orig = s->GetName();
            NStr::ReplaceInPlace(s->SetName(), "[amplified with species-specific primers", "amplified with species-specific primers");
            NStr::ReplaceInPlace(s->SetName(), "amplified with species-specific primers]", "amplified with species-specific primers");
            if (!NStr::Equal(orig, s->GetName())) {
                change = true;
                break;
            }
        }
    }

    return change;
}


DISCREPANCY_AUTOFIX(AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE)
{
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(context.FindObject(*obj));
    if (feat) {
        if (SetEnvSampleFixAmplifiedPrimers(const_cast<CSeq_feat*>(feat)->SetData().SetBiosrc())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE: Set environmental_sample, fixed amplified primers note for [n] source[s]", 1));
        }
    }
    if (desc) {
        if (SetEnvSampleFixAmplifiedPrimers(const_cast<CSeqdesc*>(desc)->SetSource())) {
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE: Set environmental_sample, fixed amplified primers note for [n] source[s]", 1));
        }
    }
    return CRef<CAutofixReport>(0);
}


// MISSING_PRIMER

DISCREPANCY_CASE(MISSING_PRIMER, BIOSRC, eOncaller, "Missing values in primer set")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->CanGetPcr_primers() && biosrc->GetPcr_primers().CanGet()) {
            bool report = false;
            for (const auto& pr : biosrc->GetPcr_primers().Get()) {
                if (pr->CanGetForward() != pr->CanGetReverse()) {
                    report = true;
                    break;
                }
                if (pr->CanGetForward()) {
                    const CPCRPrimerSet& fwdset = pr->GetForward();
                    const CPCRPrimerSet& revset = pr->GetReverse();
                    CPCRPrimerSet::Tdata::const_iterator fwd = fwdset.Get().cbegin();
                    CPCRPrimerSet::Tdata::const_iterator rev = revset.Get().cbegin();
                    while (fwd != fwdset.Get().cend() && rev != revset.Get().cend()) {
                        if (((*fwd)->CanGetName() && !(*fwd)->GetName().Get().empty()) != ((*rev)->CanGetName() && !(*rev)->GetName().Get().empty()) || ((*fwd)->CanGetSeq() && !(*fwd)->GetSeq().Get().empty()) != ((*rev)->CanGetSeq() && !(*rev)->GetSeq().Get().empty())) {
                            report = true;
                            break;
                        }
                        fwd++;
                        rev++;
                    }
                    if (report) {
                        break;
                    }
                }
            }
            if (report) {
                m_Objs["[n] biosource[s] [has] primer set[s] with missing values"].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MISSING_PRIMER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DUPLICATE_PRIMER_SET

static bool EqualPrimerSets(const CPCRPrimerSet::Tdata& a, const CPCRPrimerSet::Tdata& b)
{
    size_t count_a = 0;
    size_t count_b = 0;
    for (const auto& it : a) {
        count_a++;
    }
    for (const auto& jt : b) {
        count_b++;
    }
    if (count_a != count_b) {
        return false;
    }
    for (CPCRPrimerSet::Tdata::const_iterator it = a.cbegin(); it != a.cend(); it++) {
        CPCRPrimerSet::Tdata::const_iterator jt = b.cbegin();
        for (; jt != b.cend(); jt++) {
            if ((*it)->CanGetName() == (*jt)->CanGetName() && (*it)->CanGetSeq() == (*jt)->CanGetSeq()
                    && (!(*it)->CanGetName() || (*it)->GetName().Get() == (*jt)->GetName().Get())
                    && (!(*it)->CanGetSeq() || (*it)->GetSeq().Get() == (*jt)->GetSeq().Get())) {
                break;
            }
        }
        if (jt == b.cend()) {
            return false;
        }
    }
    return true;
}


static bool inline FindDuplicatePrimers(const CPCRReaction& a, const CPCRReaction& b)
{
    return a.CanGetForward() == b.CanGetForward() && a.CanGetReverse() == b.CanGetReverse()
        && (!a.CanGetForward() || EqualPrimerSets(a.GetForward().Get(), b.GetForward().Get()))
        && (!a.CanGetReverse() || EqualPrimerSets(a.GetReverse().Get(), b.GetReverse().Get()));
}


DISCREPANCY_CASE(DUPLICATE_PRIMER_SET, BIOSRC, eOncaller, "Duplicate PCR primer pair")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->CanGetPcr_primers() && biosrc->GetPcr_primers().CanGet()) {
            bool done = false;
            const CPCRReactionSet::Tdata data = biosrc->GetPcr_primers().Get();
            for (CPCRReactionSet::Tdata::const_iterator it = data.cbegin(); !done && it != data.cend(); it++) {
                CPCRReactionSet::Tdata::const_iterator jt = it;
                for (jt++; !done && jt != data.cend(); jt++) {
                    if (FindDuplicatePrimers(**it, **jt)) {
                        m_Objs["[n] BioSource[s] [has] duplicate primer pairs."].Add(*context.BiosourceObjRef(*biosrc));
                        done = true;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DUPLICATE_PRIMER_SET)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



// METAGENOMIC

DISCREPANCY_CASE(METAGENOMIC, BIOSRC, eDisc | eOncaller | eSmart, "Source has metagenomic qualifier")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->CanGetSubtype()) {
            for (const auto& it : biosrc->GetSubtype()) {
                if (it->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                    m_Objs["[n] biosource[s] [has] metagenomic qualifier"].Add(*context.BiosourceObjRef(*biosrc));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(METAGENOMIC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// METAGENOME_SOURCE

DISCREPANCY_CASE(METAGENOME_SOURCE, BIOSRC, eDisc | eOncaller | eSmart, "Source has metagenome_source qualifier")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetOrgname() && biosrc->GetOrg().GetOrgname().CanGetMod() && biosrc->GetOrg().IsSetTaxname() && !biosrc->GetOrg().GetTaxname().empty()) {
            for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (it->CanGetSubtype() && it->GetSubtype() == COrgMod::eSubtype_metagenome_source) {
                    m_Objs["[n] biosource[s] [has] metagenome_source qualifier"].Add(*context.BiosourceObjRef(*biosrc));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(METAGENOME_SOURCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DUP_SRC_QUAL

static string GetOrgModName(const COrgMod& qual)
{
    const COrgMod::TSubtype& subtype = qual.GetSubtype();
    return subtype == COrgMod::eSubtype_other ? "note-orgmod" : subtype == COrgMod::eSubtype_nat_host ? "host" : qual.GetSubtypeName(subtype, COrgMod::eVocabulary_raw);
}


static string GetSubtypeName(const CSubSource& qual)
{
    const CSubSource::TSubtype& subtype = qual.GetSubtype();
    return subtype == CSubSource::eSubtype_other ? "note-subsrc" : qual.GetSubtypeName(subtype, CSubSource::eVocabulary_raw);
}


static const char* kDupSrc = "[n] source[s] [has] two or more qualifiers with the same value";


DISCREPANCY_CASE(DUP_SRC_QUAL, BIOSRC, eDisc | eOncaller | eSmart, "Each qualifier on a source should have different value")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        map<string, vector<string> > Map;
        string collected_by;
        string identified_by;
        if (biosrc->CanGetSubtype()) {
            for (const auto& it : biosrc->GetSubtype()) {
                if (it->CanGetName()) {
                    const string& s = it->GetName();
                    if (it->CanGetSubtype()) {
                        if (it->GetSubtype() == CSubSource::eSubtype_collected_by) {
                            collected_by = s;
                        }
                        else if (it->GetSubtype() == CSubSource::eSubtype_identified_by) {
                            identified_by = s;
                        }
                    }
                    if (!s.empty()) {
                        Map[s].push_back(GetSubtypeName(*it));
                    }
                }
            }
        }
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetOrgname() && biosrc->GetOrg().GetOrgname().CanGetMod()) {
            for (const auto& it : biosrc->GetOrg().GetOrgname().GetMod()) {
                if (it->IsSetSubname()) {
                    const string& s = it->GetSubname();
                    if (it->CanGetSubtype()) {
                        if (it->GetSubtype() == COrgMod::eSubtype_anamorph || it->GetSubtype() == COrgMod::eSubtype_common ||
                            it->GetSubtype() == COrgMod::eSubtype_old_name || it->GetSubtype() == COrgMod::eSubtype_old_lineage ||
                            it->GetSubtype() == COrgMod::eSubtype_gb_acronym || it->GetSubtype() == COrgMod::eSubtype_gb_anamorph ||
                            it->GetSubtype() == COrgMod::eSubtype_gb_synonym) {
                            continue;
                        }
                    }
                    if (!s.empty()) {
                        Map[s].push_back(GetOrgModName(*it));
                    }
                }
            }
        }
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetTaxname()) {
            const string& s = biosrc->GetOrg().GetTaxname();
            if (!s.empty()) {
                Map[s].push_back("organism");
            }
        }
        if (biosrc->CanGetPcr_primers()) {
            for (const auto& it : biosrc->GetPcr_primers().Get()) {
                if (it->CanGetForward()) {
                    for (const auto& pr : it->GetForward().Get()) {
                        if (pr->CanGetName()) {
                            Map[pr->GetName()].push_back("fwd-primer-name");
                        }
                        if (pr->CanGetSeq()) {
                            Map[pr->GetSeq()].push_back("fwd-primer-seq");
                        }
                    }
                }
                if (it->CanGetReverse()) {
                    for (const auto& pr : it->GetReverse().Get()) {
                        if (pr->CanGetName()) {
                            Map[pr->GetName()].push_back("rev-primer-name");
                        }
                        if (pr->CanGetSeq()) {
                            Map[pr->GetSeq()].push_back("rev-primer-seq");
                        }
                    }
                }
            }
        }
        bool bad = false;
        for (const auto& it : Map) {
            if (it.second.size() > 1) {
                if (it.second.size() == 2 && it.first == collected_by && collected_by == identified_by) {
                    continue; // there is no error if collected_by equals to identified_by
                }
                string s = "[n] biosource[s] [has] value\'";
                s += it.first;
                s += "\' for these qualifiers: ";
                for (size_t i = 0; i < it.second.size(); i++) {
                    if (i) {
                        s += ", ";
                    }
                    s += it.second[i];
                }
                m_Objs[kDupSrc][s].Add(*context.BiosourceObjRef(*biosrc));
            }
        }
        if (bad) {
            m_Objs[kDupSrc].Incr();
        }
    }
}


DISCREPANCY_SUMMARIZE(DUP_SRC_QUAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_ALIAS(DUP_SRC_QUAL, DUP_SRC_QUAL_DATA)


// UNUSUAL_ITS

DISCREPANCY_CASE(UNUSUAL_ITS, SEQUENCE, eDisc | eOncaller, "Test Bioseqs for unusual rRNA / ITS")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (context.HasLineage(biosrc ? &biosrc->GetSource() : 0, "Microsporidia")) {
        bool has_unusual = false;
        for (const CSeq_feat& feat : context.GetFeat()) {
            if (feat.IsSetComment() && feat.IsSetData() && feat.GetData().IsRna()) {
                const CRNA_ref& rna = feat.GetData().GetRna();
                if (rna.IsSetType() && rna.GetType() == CRNA_ref::eType_miscRNA) {
                    if (NStr::StartsWith(feat.GetComment(), "contains", NStr::eNocase)) {
                        has_unusual = true;
                        break;
                    }
                }
            }
        }
        if (has_unusual) {
            m_Objs["[n] Bioseq[s] [has] unusual rRNA / ITS"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(UNUSUAL_ITS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SARS_QUALS

#define SARS_TAX_ID 2697049

DISCREPANCY_CASE(SARS_QUALS, BIOSRC, eOncaller, "SARS-CoV-2 isolate must have correct format")
{
    for (const CBioSource* biosrc : context.GetBiosources()) {
        if (biosrc->IsSetOrg() && biosrc->GetOrg().CanGetDb()) {
            bool sars = false;
            for (const auto& db : biosrc->GetOrg().GetDb()) {
                if (db->CanGetTag() && db->GetTag().IsId() && db->GetTag().GetId() == SARS_TAX_ID && db->CanGetDb() && db->GetDb() == "taxon") {
                    sars = true;
                    break;
                }
            }
            if (sars) {
                string isolate;
                bool good = true;
                if (biosrc->IsSetOrg() && biosrc->GetOrg().IsSetOrgname() && biosrc->GetOrg().GetOrgname().IsSetMod()) {
                    for (const auto& m : biosrc->GetOrg().GetOrgname().GetMod()) {
                        if (m->IsSetSubtype() && m->GetSubtype() == COrgMod::eSubtype_isolate && m->IsSetSubname()) {
                            isolate = m->GetSubname();
                            break;
                        }
                    }
                }
                if (!isolate.length()) {
                    isolate = "no isolate";
                    good = false;
                }
                if (good && isolate.find("SARS-CoV-2") != 0) {
                    good = false;
                }
                string year;
                if (good) {
                    vector<string> arr;
                    NStr::SplitByPattern(isolate, "/", arr);
                    if (arr.size() == 5) {
                        year = arr[4];
                    }
                    else {
                        good = false;
                    }
                }
                if (good) {
                    string date, date0, date1;
                    if (biosrc->IsSetSubtype()) {
                        for (const auto& it : biosrc->GetSubtype()) {
                            if (it->IsSetSubtype() && it->GetSubtype() == CSubSource::eSubtype_collection_date && it->IsSetName()) {
                                date = it->GetName();
                                break;
                            }
                        }
                    }
                    if (date.length() >= 4) {
                        date0 = date.substr(0, 4);
                        date1 = date.substr(date.length() - 4);
                    }
                    good = year == date0 || year == date1;
                }
                if (!good) {
                    m_Objs["[n] SARS-CoV-2 biosource[s] [has] wrong isolate format"][isolate].Add(*context.BiosourceObjRef(*biosrc));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SARS_QUALS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
