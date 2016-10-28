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
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/macro/Source_qual.hpp>
#include <objects/taxon3/taxon3.hpp>

#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(biosource_tests);

static unsigned int AutofixBiosrc(TReportObjectList& list, CScope& scope, bool (*call)(CBioSource& src))
{
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        const CSeqdesc* csd = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            if (sf->IsSetData() && sf->GetData().IsBiosrc()) {
                CRef<CSeq_feat> new_feat(new CSeq_feat());
                new_feat->Assign(*sf);
                if (call(new_feat->SetData().SetBiosrc())) {
                    CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
                    feh.Replace(*new_feat);
                    n++;
                }
            }
        }
        else if (csd) {
            if (csd->IsSource()) {
                CSeqdesc* sd = const_cast<CSeqdesc*>(csd);
                if (call(sd->SetSource())) {
                    n++;
                }
            }
        }
    }
    return n;
}


// MAP_CHROMOSOME_CONFLICT

const string kMapChromosomeConflict = "[n] source[s] on eukaryotic sequence[s] [has] map but not chromosome";


DISCREPANCY_CASE(MAP_CHROMOSOME_CONFLICT, CSeqdesc, eDisc | eOncaller | eSmart, "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier")
{
    if (!obj.IsSource() || !context.IsEukaryotic() || !obj.GetSource().IsSetSubtype()) {
        return;
    }

    bool has_map = false;
    bool has_chromosome = false;

    ITERATE (CBioSource::TSubtype, it, obj.GetSource().GetSubtype()) {
        if ((*it)->IsSetSubtype()) {
            if ((*it)->GetSubtype() == CSubSource::eSubtype_map) {
                has_map = true;
            } else if ((*it)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                has_chromosome = true;
                break;
            }
        } 
    }

    if (has_map && !has_chromosome) {
        m_Objs[kMapChromosomeConflict].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(&obj), context.GetCurrentBioseqLabel()), false).Fatal();
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MAP_CHROMOSOME_CONFLICT)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INFLUENZA_DATE_MISMATCH
/* For Influenza A viruses, the year of the collection date should be
* the last number before the second set of parentheses in the tax name.
* For Influenza B viruses, the year should be the last token in the
* tax name (tokens are separated by / characters).
*/
static int GetYearFromInfluenzaA(const string& taxname)
{
    size_t pos = NStr::Find(taxname, "(");
    if (pos == string::npos) {
        return 0;
    }
    pos = NStr::Find(taxname, "(", pos + 1);
    if (pos == string::npos) {
        return 0;
    }
    pos--;
    while (pos > 0 && isspace(taxname.c_str()[pos])) {
        pos--;
    }
    if (pos < 4 ||
        !isdigit(taxname.c_str()[pos]) ||
        !isdigit(taxname.c_str()[pos - 1]) ||
        !isdigit(taxname.c_str()[pos - 2]) ||
        !isdigit(taxname.c_str()[pos - 3])) {
        return 0;
    }
    return NStr::StringToInt(taxname.substr(pos - 3, 4));
}


static int GetYearFromInfluenzaB(const string& taxname)
{
    size_t pos = taxname.rfind('/');
    if (pos == string::npos) {
        return 0;
    }
    ++pos;
    while (isspace(taxname.c_str()[pos])) {
        ++pos;
    }
    size_t len = 0;
    while (isdigit(taxname.c_str()[pos + len])) {
        len++;
    }
    if (len > 0) {
        return NStr::StringToInt(taxname.substr(pos, len));
    } else {
        return 0;
    }
}


static bool DoInfluenzaStrainAndCollectionDateMisMatch(const CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetTaxname()) {
        return false;
    }
    
    int year = 0;
    if (NStr::StartsWith(src.GetOrg().GetTaxname(), "Influenza A virus ")) {
        year = GetYearFromInfluenzaA(src.GetOrg().GetTaxname());
    } else if (NStr::StartsWith(src.GetOrg().GetTaxname(), "Influenza B virus ")) {
        year = GetYearFromInfluenzaB(src.GetOrg().GetTaxname());
    } else {
        // not influenza A or B, no mismatch can be calculated
        return false;
    }

    if (year > 0 && src.IsSetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
            if ((*it)->IsSetSubtype() &&
                (*it)->GetSubtype() == CSubSource::eSubtype_collection_date &&
                (*it)->IsSetName()) {
                try {
                    CRef<CDate> date = CSubSource::DateFromCollectionDate((*it)->GetName());
                    if (date->IsStd() && date->GetStd().IsSetYear() && date->GetStd().GetYear() == year) {
                        // match found, no mismatch
                        return false;
                    } else {
                        return true;
                    }
                } catch (CException) {
                    //unable to parse date, assume mismatch
                    return true;
                }
            }
        }
    }
    return true;
}


const string kInfluenzaDateMismatch = "[n] influenza strain[s] conflict with collection date";
DISCREPANCY_CASE(INFLUENZA_DATE_MISMATCH, CBioSource, eOncaller, "Influenza Strain/Collection Date Mismatch")
{
    if (DoInfluenzaStrainAndCollectionDateMisMatch(obj)) {
        m_Objs[kInfluenzaDateMismatch].Add(*context.NewFeatOrDescObj(), false).Fatal();
    }
}


DISCREPANCY_SUMMARIZE(INFLUENZA_DATE_MISMATCH)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// UNCULTURED_NOTES

static bool HasUnculturedNotes(const CBioSource& src)
{
    if (!src.IsSetSubtype()) {
        return false;
    }
    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        if ((*it)->IsSetSubtype() &&
            (*it)->GetSubtype() == CSubSource::eSubtype_other &&
            (*it)->IsSetName() &&
            CSubSource::HasCultureNotes((*it)->GetName())) {
            return true;
        }
    }
    return false;
};


const string kUnculturedNotes = "[n] bio-source[s] [has] uncultured note[s]";

DISCREPANCY_CASE(UNCULTURED_NOTES, CBioSource, eOncaller, "Uncultured Notes")
{
    if (HasUnculturedNotes(obj)) {
        m_Objs[kUnculturedNotes].Add(*context.NewFeatOrDescObj(), false).Fatal();
    }
}


DISCREPANCY_SUMMARIZE(UNCULTURED_NOTES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_VIRAL_QUALS

const string kMissingViralQualsTop = "[n] virus organism[s] [is] missing required qualifiers";
const string kMissingViralQualsCollectionDate = "[n] virus organism[s] [is] missing suggested qualifier collection date";
const string kMissingViralQualsCountry = "[n] virus organism[s] [is] missing suggested qualifier country";
const string kMissingViralQualsSpecificHost = "[n] virus organism[s] [is] missing suggested qualifier specific-host";

DISCREPANCY_CASE(MISSING_VIRAL_QUALS, CBioSource, eOncaller, "Viruses should specify collection-date, country, and specific-host")
{
    if (!CDiscrepancyContext::HasLineage(obj, context.GetLineage(), "Viruses")) {
        return;
    }
    bool has_collection_date = false;
    bool has_country = false;
    bool has_specific_host = false;
    if (obj.IsSetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, obj.GetSubtype()) {
            if ((*it)->IsSetSubtype()) {
                if ((*it)->GetSubtype() == CSubSource::eSubtype_collection_date) {
                    has_collection_date = true;
                } else if ((*it)->GetSubtype() == CSubSource::eSubtype_country) {
                    has_country = true;
                }
                if (has_collection_date && has_country) {
                    break;
                }
            }
        }
    }
    if (obj.IsSetOrg() && obj.GetOrg().IsSetOrgname() && obj.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_nat_host) {
                has_specific_host = true;
            }
        }
    }
    if (!has_collection_date || !has_country || !has_specific_host) {
        if (!has_collection_date) {
            m_Objs[kMissingViralQualsTop][kMissingViralQualsCollectionDate].Ext().Add(*context.NewFeatOrDescObj(), false);
        }
        if (!has_country) {
            m_Objs[kMissingViralQualsTop][kMissingViralQualsCountry].Ext().Add(*context.NewFeatOrDescObj(), false);
        }
        if (!has_specific_host) {
            m_Objs[kMissingViralQualsTop][kMissingViralQualsSpecificHost].Ext().Add(*context.NewFeatOrDescObj(), false);
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

    ITERATE (COrgName::TMod, m, mods) {
        if ((*m)->IsSetSubtype() &&
            (*m)->GetSubtype() == COrgMod::eSubtype_culture_collection &&
            (*m)->IsSetSubname() &&
            NStr::StartsWith((*m)->GetSubname(), "ATCC:")) {
            string cmp = (*m)->GetSubname().substr(5);
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

    ITERATE (COrgName::TMod, m, mods) {
        if ((*m)->IsSetSubtype() &&
            (*m)->GetSubtype() == COrgMod::eSubtype_strain &&
            (*m)->IsSetSubname() &&
            NStr::StartsWith((*m)->GetSubname(), "ATCC ")) {
            string cmp = (*m)->GetSubname().substr(5);
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


const string kATCCCultureConflict = "[n] biosource[s] [has] conflicting ATCC strain and culture collection values";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(ATCC_CULTURE_CONFLICT, CBioSource, eDisc | eOncaller, "ATCC strain should also appear in culture collection")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }

    bool report = false;

    ITERATE (COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*m)->IsSetSubtype() && 
            (*m)->IsSetSubname()) {
            if ((*m)->GetSubtype() == COrgMod::eSubtype_strain &&
                NStr::StartsWith((*m)->GetSubname(), "ATCC ") &&
                !HasCultureCollectionForATCCStrain(obj.GetOrg().GetOrgname().GetMod(), (*m)->GetSubname().substr(5))) {
                report = true;
                break;
            } else if ((*m)->GetSubtype() == COrgMod::eSubtype_culture_collection &&
                NStr::StartsWith((*m)->GetSubname(), "ATCC:") &&
                !HasStrainForATCCCultureCollection(obj.GetOrg().GetOrgname().GetMod(), (*m)->GetSubname().substr(5))) {
                report = true;
                break;
            }
        }
    }
    if (report) {
        m_Objs[kATCCCultureConflict].Add(*context.NewFeatOrDescObj());
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(ATCC_CULTURE_CONFLICT)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool SetCultureCollectionFromStrain(CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetOrgMod() || !src.GetOrg().GetOrgname().IsSetMod()) {
        return false;
    }

    vector<string> add;
    ITERATE (COrgName::TMod, m, src.GetOrg().GetOrgname().GetMod()) {
        if ((*m)->IsSetSubtype() &&
            (*m)->IsSetSubname()) {
            if ((*m)->GetSubtype() == COrgMod::eSubtype_strain &&
                NStr::StartsWith((*m)->GetSubname(), "ATCC ") &&
                !HasCultureCollectionForATCCStrain(src.GetOrg().GetOrgname().GetMod(), (*m)->GetSubname().substr(5))) {
                add.push_back("ATCC:" + (*m)->GetSubname());
            }
        }
    }
    if (!add.empty()) {
        ITERATE (vector<string>, s, add) {
            CRef<COrgMod> m(new COrgMod(COrgMod::eSubtype_culture_collection, *s));
            src.SetOrg().SetOrgname().SetMod().push_back(m);
        }
        return true;
    }
    return false;
}


DISCREPANCY_AUTOFIX(ATCC_CULTURE_CONFLICT)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = AutofixBiosrc(list, scope, SetCultureCollectionFromStrain);
    return CRef<CAutofixReport>(n ? new CAutofixReport("ATCC_CULTURE_CONFLICT: Set culture collection for [n] source[s]", n) : 0);
}

// BACTERIA_MISSING_STRAIN
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BACTERIA_MISSING_STRAIN, CBioSource, eDisc | eOncaller | eSubmitter | eSmart, "Missing strain on bacterial 'Genus sp. strain'")
//  ----------------------------------------------------------------------------
{
    // only looking for bacteria
    if (!CDiscrepancyContext::HasLineage(obj, context.GetLineage(), "Bacteria")) {
        return;
    }

    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetTaxname() || 
        NStr::FindNoCase(obj.GetOrg().GetTaxname(), "enrichment culture clone") != string::npos) {
        // ignore enrichment culture clones, no taxname at all
        return;
    }
    size_t pos = NStr::Find(obj.GetOrg().GetTaxname(), " sp. ");
    if (pos == string::npos) {
        // only looking for sp.
        return;
    }

    // ignore if parentheses after sp.
    if (NStr::EndsWith(obj.GetOrg().GetTaxname(), ")") &&
        NStr::Find(obj.GetOrg().GetTaxname(), "(", pos + 5) != string::npos) {
        return;
    }

    bool found = false;
    string cmp = obj.GetOrg().GetTaxname().substr(pos + 5);
    
    if (obj.IsSetOrg() && obj.GetOrg().IsSetOrgname() && obj.GetOrgname().IsSetMod()) {
        ITERATE (COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*m)->IsSetSubtype() &&
                (*m)->GetSubtype() == COrgMod::eSubtype_strain &&
                (*m)->IsSetSubname() &&
                NStr::Equal((*m)->GetSubname(), cmp)) {
                found = true;
            }
        }
    }
    if (!found) {
        m_Objs["[n] bacterial biosource[s] [has] taxname 'Genus sp. strain' but no strain"].Add(*context.NewFeatOrDescObj());
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BACTERIA_MISSING_STRAIN)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BACTERIA_SHOULD_NOT_HAVE_ISOLATE
const string kAmplifiedWithSpeciesSpecificPrimers = "amplified with species-specific primers";
bool HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& src)
{
    if (src.IsSetSubtype()) {
        ITERATE (CBioSource::TSubtype, s, src.GetSubtype()) {
            if ((*s)->IsSetSubtype() && (*s)->GetSubtype() == CSubSource::eSubtype_other &&
                (*s)->IsSetName() && NStr::Equal((*s)->GetName(), kAmplifiedWithSpeciesSpecificPrimers)) {
                return true;
            }
        }
    }

    if (src.IsSetOrg() && src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE (COrgName::TMod, m, src.GetOrg().GetOrgname().GetMod()) {
            if ((*m)->IsSetSubtype() && (*m)->GetSubtype() == CSubSource::eSubtype_other &&
                (*m)->IsSetSubname() && NStr::Equal((*m)->GetSubname(), kAmplifiedWithSpeciesSpecificPrimers)) {
                return true;
            }
        }
    }

    return false;
}


DISCREPANCY_CASE(BACTERIA_SHOULD_NOT_HAVE_ISOLATE, CBioSource, eOncaller, "Bacterial sources should not have isolate")
{
    // only looking for bacteria
    if (!CDiscrepancyContext::HasLineage(obj, context.GetLineage(), "Bacteria")) {
        return;
    }
    if (HasAmplifiedWithSpeciesSpecificPrimerNote(obj)) {
        return;
    }

    bool has_bad_isolate = false;
    if (obj.IsSetOrg() && obj.GetOrg().IsSetOrgname() && obj.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE (COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*m)->IsSetSubtype() && (*m)->GetSubtype() == COrgMod::eSubtype_isolate &&
                (*m)->IsSetSubname() && 
                !NStr::StartsWith((*m)->GetSubname(), "DGGE gel band") &&
                !NStr::StartsWith((*m)->GetSubname(), "TGGE gel band") &&
                !NStr::StartsWith((*m)->GetSubname(), "SSCP gel band")) {
                has_bad_isolate = true;
                break;
            }
        }
    }
    if (has_bad_isolate) {
        m_Objs["[n] bacterial biosource[s] [has] isolate"].Add(*context.NewFeatOrDescObj());
    }
}


DISCREPANCY_SUMMARIZE(BACTERIA_SHOULD_NOT_HAVE_ISOLATE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MULTISRC

DISCREPANCY_CASE(MULTISRC, CBioSource, eDisc | eOncaller, "Comma or semicolon appears in strain or isolate")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    bool report = false;
    ITERATE (COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*m)->IsSetSubtype() &&
            ((*m)->GetSubtype() == COrgMod::eSubtype_isolate || (*m)->GetSubtype() == COrgMod::eSubtype_strain) &&
            (*m)->IsSetSubname() &&
            (NStr::Find((*m)->GetSubname(), ",") != string::npos ||
             NStr::Find((*m)->GetSubname(), ";") != string::npos)) {
            report = true;
            break;
        }
    }
    if (report) {
        m_Objs["[n] organism[s] [has] comma or semicolon in strain or isolate"].Add(*context.NewFeatOrDescObj());
    }
}


DISCREPANCY_SUMMARIZE(MULTISRC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MULTIPLE_CULTURE_COLLECTION

DISCREPANCY_CASE(MULTIPLE_CULTURE_COLLECTION, CBioSource, eOncaller, "Multiple culture-collection quals")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    bool found = false;
    ITERATE (COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*m)->IsSetSubtype() && (*m)->GetSubtype() == COrgMod::eSubtype_culture_collection) {
            if (found) {
                m_Objs["[n] organism[s] [has] multiple culture-collection qualifiers"].Add(*context.NewFeatOrDescObj());
                return;
            }
            found = true;
        }
    }
}


DISCREPANCY_SUMMARIZE(MULTIPLE_CULTURE_COLLECTION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// REQUIRED_STRAIN

DISCREPANCY_CASE(REQUIRED_STRAIN, CBioSource, eDisc, "Bacteria should have strain")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !CDiscrepancyContext::HasLineage(obj, context.GetLineage(), "Bacteria")) {
        return;
    }

    if (obj.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE(COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*m)->IsSetSubtype() && (*m)->GetSubtype() == COrgMod::eSubtype_strain) {
                return;
            }
        }
    }
    m_Objs["[n] biosource[s] [is] missing required strain value"].Add(*context.NewFeatOrDescObj());
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


DISCREPANCY_CASE(STRAIN_CULTURE_COLLECTION_MISMATCH, CBioSource, eOncaller | eSmart, "Strain and culture-collection values conflict")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    const COrgName::TMod& mod = obj.GetOrg().GetOrgname().GetMod();
    bool conflict = false;
    ITERATE (COrgName::TMod, m, mod) {
        if ((*m)->IsSetSubtype() && (*m)->GetSubtype() == COrgMod::eSubtype_strain) {
            COrgName::TMod::const_iterator j = m;
            for (++j; j != mod.end(); ++j) {
                if ((*j)->IsSetSubtype() && (*j)->GetSubtype() == COrgMod::eSubtype_culture_collection) {
                    if(MatchExceptSpaceColon((*m)->GetSubname(), (*j)->GetSubname())) {
                        return;
                    }
                    else {
                        conflict = true;
                    }
                }
            }
        }
    }
    if (conflict) {
        m_Objs["[n] organism[s] [has] conflicting strain and culture-collection values"].Add(*context.NewFeatOrDescObj());
    }
}


DISCREPANCY_SUMMARIZE(STRAIN_CULTURE_COLLECTION_MISMATCH)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SP_NOT_UNCULTURED

DISCREPANCY_CASE(SP_NOT_UNCULTURED, CBioSource, eOncaller, "Organism ending in sp. needs tax consult")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().CanGetTaxname()) {
        return;
    }
    const string& s = obj.GetOrg().GetTaxname();
    if (s.length() > 4 && s.substr(s.length() - 4) == " sp." && s.substr(0, 11) != "uncultured ") {
        m_Objs["[n] biosource[s] [has] taxname[s] that end[S] with \' sp.\' but [does] not start with \'uncultured\'"].Add(*context.NewFeatOrDescObj());
    }
}


DISCREPANCY_SUMMARIZE(SP_NOT_UNCULTURED)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_STRAND_TRNAS

const string kMinusStrand = "[n] tRNA[s] on minus strand";
const string kPlusStrand = "[n] tRNA[s] on plus strand";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(FIND_STRAND_TRNAS, CBioSource, eDisc, "Find tRNAs on the same strand")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetGenome()) {
        return;
    }

    CBioSource::TGenome genome = obj.GetGenome();
    if (genome != CBioSource::eGenome_mitochondrion && genome != CBioSource::eGenome_chloroplast && genome != CBioSource::eGenome_plastid) {
        return;
    }

    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if (!bioseq || !bioseq->IsSetAnnot()) {
        return;
    }

    const CSeq_annot* annot = nullptr;
    ITERATE (CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
        if ((*annot_it)->IsFtable()) {
            annot = *annot_it;
            break;
        }
    }

    if (annot) {
        bool mixed_strand = false,
            first = true;

        ENa_strand strand = eNa_strand_unknown;

        list< CConstRef< CSeq_feat > > trnas;
        ITERATE (CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {

            if ((*feat)->IsSetLocation() && (*feat)->IsSetData() && (*feat)->GetData().IsRna()) {

                const CSeqFeatData::TRna& rna = (*feat)->GetData().GetRna();

                if (rna.IsSetType() && rna.GetType() == CRNA_ref::eType_tRNA) {

                    if (first) {
                        strand = (*feat)->GetLocation().GetStrand();
                        first = false;
                    }
                    else {
                        ENa_strand cur_strand = (*feat)->GetLocation().GetStrand();
                        if ((strand == eNa_strand_minus && cur_strand != eNa_strand_minus)
                            || (strand != eNa_strand_minus && cur_strand == eNa_strand_minus))
                            mixed_strand = true;
                    }

                    trnas.push_back(CConstRef<CSeq_feat>(*feat));
                }
            }

            if (mixed_strand) {
                break;
            }
        }

        if (!mixed_strand && !trnas.empty()) {
            const string& msg = (strand == eNa_strand_minus) ? kMinusStrand : kPlusStrand;

            for (auto trna = trnas.begin(); trna != trnas.end(); ++trna) {
                m_Objs[msg].Add(*context.NewDiscObj(*trna), false);
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(FIND_STRAND_TRNAS)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// REQUIRED_CLONE

static bool IsMissingRequiredClone(const CBioSource& biosource)
{
    if (HasAmplifiedWithSpeciesSpecificPrimerNote(biosource)) {
        return false;
    }

    bool needs_clone = biosource.IsSetOrg() && biosource.GetOrg().IsSetTaxname() &&
                       NStr::StartsWith(biosource.GetOrg().GetTaxname(), "uncultured", NStr::eNocase);

    bool has_clone = false;

    if (biosource.IsSetSubtype()) {

        ITERATE (CBioSource::TSubtype, subtype_it, biosource.GetSubtype())
        {
            if ((*subtype_it)->IsSetSubtype()) {

                CSubSource::TSubtype subtype = (*subtype_it)->GetSubtype();

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

            ITERATE (COrgName::TMod, mod_it, biosource.GetOrg().GetOrgname().GetMod()) {

                if ((*mod_it)->IsSetSubtype() && (*mod_it)->GetSubtype() == COrgMod::eSubtype_isolate) {
                    if ((*mod_it)->IsSetSubname() && NStr::FindNoCase((*mod_it)->GetSubname(), "gel band") != NPOS) {
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

const string kMissingRequiredClone = "[n] biosource[s] [is] missing required clone value";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(REQUIRED_CLONE, CBioSource, eOncaller, "Uncultured or environmental sources should have clone")
//  ----------------------------------------------------------------------------
{
    if (IsMissingRequiredClone(obj)) {
        m_Objs[kMissingRequiredClone].Add(*context.NewFeatOrDescObj());
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(REQUIRED_CLONE)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// STRAIN_TAXNAME_MISMATCH

string GetTaxnameFromReportObject(const CReportObj& obj)
{
    string taxname = kEmptyStr;
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(obj.GetObject().GetPointer());
    if (desc) {
        if (desc->IsSource() && desc->GetSource().IsSetOrg() &&
            desc->GetSource().GetOrg().IsSetTaxname()) {
            taxname = desc->GetSource().GetOrg().GetTaxname();
        }
    } else {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(obj.GetObject().GetPointer());
        if (sf && sf->IsSetData() && sf->GetData().IsBiosrc() &&
            sf->GetData().GetBiosrc().IsSetOrg() &&
            sf->GetData().GetBiosrc().GetOrg().IsSetTaxname()) {
            taxname = sf->GetData().GetBiosrc().GetOrg().GetTaxname();
        }
    }
    return taxname;
}


bool DoTaxnamesMatchForObjectList(const TReportObjectList& obj_list)
{
    TReportObjectList::const_iterator ro = obj_list.cbegin();
    string taxname = GetTaxnameFromReportObject(**ro);
    ++ro;
    while (ro != obj_list.cend()) {
        string this_taxname = GetTaxnameFromReportObject(**ro);
        if (!NStr::Equal(taxname, this_taxname)) {
            return false;
        }
        ++ro;
    }
    return true;
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(STRAIN_TAXNAME_MISMATCH, CBioSource, eDisc | eOncaller, "BioSources with the same strain should have the same taxname")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    ITERATE (COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_strain &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            m_Objs["[n] biosource[s] have strain " + (*om)->GetSubname() + " but do not have the same taxnames"].Add(*context.NewFeatOrDescObj(eKeepRef)); // should be optimized!!!
        }
    }
}


void SummarizeTaxnameConflict(CReportNode& m_Objs, const string& qual_name)
{
    CReportNode::TNodeMap::iterator it = m_Objs.GetMap().begin();
    while (it != m_Objs.GetMap().end()) {
        if (it->second->GetObjects().size() < 2) {
            // only one biosource with this strain
            it = m_Objs.GetMap().erase(it);
        } else if (DoTaxnamesMatchForObjectList(it->second->GetObjects())) {
            // taxnames match
            it = m_Objs.GetMap().erase(it);
        } else {
            ++it;
        }
    }

    if (m_Objs.GetMap().size() > 1) {
        // add top level category
        size_t num_biosources = 0;
        NON_CONST_ITERATE (CReportNode::TNodeMap, it, m_Objs.GetMap()) {
            num_biosources += it->second->GetObjects().size();
        }
        string new_label = NStr::NumericToString(num_biosources) +
            " BioSources have " + qual_name + "/taxname conflicts";

        NON_CONST_ITERATE (CReportNode::TNodeMap, it, m_Objs.GetMap()) {
            NON_CONST_ITERATE (TReportObjectList, q, it->second->GetObjects()) {
                m_Objs[new_label][it->first].Add(**q).Ext();
            }
        }
        CReportNode::TNodeMap::iterator it = m_Objs.GetMap().begin();
        while (it != m_Objs.GetMap().end()) {
            if (!NStr::Equal(it->first, new_label)) {
                it = m_Objs.GetMap().erase(it);
            } else {
                ++it;
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(STRAIN_TAXNAME_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    SummarizeTaxnameConflict(m_Objs, "strain");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SPECVOUCHER_TAXNAME_MISMATCH
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SPECVOUCHER_TAXNAME_MISMATCH, CBioSource, eOncaller | eSmart, "BioSources with the same specimen voucher should have the same taxname")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    ITERATE (COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_specimen_voucher &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            m_Objs["[n] biosource[s] have specimen voucher " + (*om)->GetSubname() + " but do not have the same taxnames"].Add(*context.NewFeatOrDescObj(eKeepRef));
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SPECVOUCHER_TAXNAME_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    SummarizeTaxnameConflict(m_Objs, "specimen voucher");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CULTURE_TAXNAME_MISMATCH
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CULTURE_TAXNAME_MISMATCH, CBioSource, eOncaller, "Test BioSources with the same culture collection but different taxname")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    ITERATE (COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_culture_collection &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            m_Objs["[n] biosource[s] have culture collection " + (*om)->GetSubname() + " but do not have the same taxnames"].Add(*context.NewFeatOrDescObj(eKeepRef));
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CULTURE_TAXNAME_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    SummarizeTaxnameConflict(m_Objs, "culture collection");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BIOMATERIAL_TAXNAME_MISMATCH
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BIOMATERIAL_TAXNAME_MISMATCH, CBioSource, eOncaller | eSmart, "Test BioSources with the same biomaterial but different taxname")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    ITERATE (COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_bio_material &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            m_Objs["[n] biosource[s] have biomaterial " + (*om)->GetSubname() + " but do not have the same taxnames"].Add(*context.NewFeatOrDescObj(eKeepRef));
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BIOMATERIAL_TAXNAME_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    SummarizeTaxnameConflict(m_Objs, "biomaterial");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ORGANELLE_ITS
const string kSuspectITS = "[n] Bioseq[s] [has] suspect rRNA / ITS on organelle";


DISCREPANCY_CASE(ORGANELLE_ITS, CBioSource, eOncaller, "Test Bioseqs for suspect rRNA / ITS on organelle")
{
    if (!obj.IsSetGenome() || !(obj.GetGenome() == CBioSource::eGenome_chloroplast || obj.GetGenome() == CBioSource::eGenome_mitochondrion)) {
        return;
    }

    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if (!bioseq || !bioseq->IsSetAnnot()) {
        return;
    }

    const CSeq_annot* annot = nullptr;
    ITERATE (CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
        if ((*annot_it)->IsFtable()) {
            annot = *annot_it;
            break;
        }
    }

    bool has_suspect = false;

    if (annot) {
        ITERATE (CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {
            if ((*feat)->IsSetData() && (*feat)->GetData().IsRna()) {

                const CRNA_ref& rna = (*feat)->GetData().GetRna();

                if (rna.IsSetType() && (rna.GetType() == CRNA_ref::eType_rRNA || rna.GetType() == CRNA_ref::eType_miscRNA)) {

                    static vector<string> suspectable_products = {
                        "18S ribosomal RNA",
                        "5.8S ribosomal RNA",
                        "26S ribosomal RNA",
                        "28S ribosomal RNA",
                        "internal transcribed spacer 1",
                        "internal transcribed spacer 2"
                    };

                    string product = rna.GetRnaProductName();
                    if (NStr::FindNoCase(suspectable_products, product) != nullptr) {
                        has_suspect = true;
                        break;
                    }

                    if ((*feat)->IsSetComment()) {
                        string comment = (*feat)->GetComment();
                        if (!comment.empty()) {

                            for (auto suspectable = suspectable_products.begin(); suspectable != suspectable_products.end(); ++suspectable) {
                                if (NStr::FindNoCase(comment, *suspectable) != NPOS) {
                                    has_suspect = true;
                                    break;
                                }
                            }

                            if (has_suspect) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (has_suspect) {
        m_Objs[kSuspectITS].Add(*context.NewBioseqObj(bioseq, &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(ORGANELLE_ITS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// HAPLOTYPE_MISMATCH

static const string kTaxnameAndHaplotype = "TaxnameAndHaplotype";

DISCREPANCY_CASE(HAPLOTYPE_MISMATCH, CBioSource, eOncaller, "Sequences with the same haplotype should match")
{
    if (!obj.IsSetTaxname() || !obj.IsSetSubtype()) {
        return;
    }

    ITERATE (CBioSource::TSubtype, subtype, obj.GetSubtype()) {
        if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_haplotype) {
            m_Objs[kTaxnameAndHaplotype].Add(*context.NewBioseqObj(context.GetCurrentBioseq(), &context.GetSeqSummary(), eKeepRef), true);
            break;
        }
    }
}

typedef list<CBioseq_Handle> TBioseqHandleList;
typedef map<pair<string, string>, TBioseqHandleList> THaplotypeMap;

static string GetHaplotype(const CBioSource& biosource)
{
    string res;
    ITERATE (CBioSource::TSubtype, subtype, biosource.GetSubtype()) {
        if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_haplotype && (*subtype)->IsSetName()) {
            res = (*subtype)->GetName();
            break;
        }
    }

    return res;
}

static void CollectBioseqsByTaxnameAndHaplotype(CDiscrepancyContext& context, TReportObjectList& bioseqs, THaplotypeMap& haplotypes)
{
    NON_CONST_ITERATE (TReportObjectList, bioseq, bioseqs) {
        const CDiscrepancyObject* dobj = dynamic_cast<const CDiscrepancyObject*>(bioseq->GetPointer());
        if (dobj) {
            const CBioseq* cur_bioseq = dobj->GetBioseq();
            if (cur_bioseq) {
                CBioseq_Handle bioseq_h = context.GetScope().GetBioseqHandle(*cur_bioseq);
                CSeqdesc_CI biosrc(bioseq_h, CSeqdesc::e_Source);
                if (biosrc) {
                    const CBioSource& biosource = biosrc->GetSource();
                    string taxname = biosource.GetTaxname(),
                        haplotype = GetHaplotype(biosource);
                    if (!taxname.empty() && !haplotype.empty()) {
                        haplotypes[make_pair(taxname, haplotype)].push_back(bioseq_h);
                    }
                }
            }
        }
    }
}

enum EHaplotypeProblem
{
    noProblem,
    strictMatchProblem,
    nonStrictMatchProblem
};

static bool IsEqualSubSeq(const CSeqVector& seq1, const CSeqVector& seq2, size_t start1, size_t start2, size_t len, bool allowN)
{
    for (size_t i = 0; i < len; ++i) {

        if (allowN && (seq1.IsInGap(start1 + i) || seq2.IsInGap(start2 + i))) {
            continue;
        }

        if (seq1[start1 + i] != seq2[start2 + i]) {
            return false;
        }
    }

    return true;
}

static bool IsSeqOverlap(const CSeqVector& seq1, const CSeqVector& seq2, bool allowN)
{
    size_t seq1_size = seq1.size(),
           seq2_size = seq2.size(),
           min_size = min(seq1_size, seq2_size);

    for (size_t cur_size = min_size / 2 + 1; // minimum size of an overlapping area
         cur_size <= min_size;
         ++cur_size) {

        if (IsEqualSubSeq(seq1, seq2, seq1_size - cur_size, 0, cur_size, allowN)) {
            return true;
        }
    }

    return false;
}

static bool IsSeqEqualOrOverlap(const CBioseq_Handle& bioseq1, const CBioseq_Handle& bioseq2, bool allowN)
{
    CSeqVector seq1(bioseq1, CBioseq_Handle::eCoding_Ncbi, eNa_strand_plus),
               seq2(bioseq2, CBioseq_Handle::eCoding_Ncbi, eNa_strand_plus);

    bool res = false;
    // Check for equality at first
    if (seq1.size() == seq2.size()) {
        res = IsEqualSubSeq(seq1, seq2, 0, 0, seq1.size(), allowN);
    }

    // Overlap check
    if (!res) {
        res = IsSeqOverlap(seq1, seq2, allowN) || IsSeqOverlap(seq2, seq1, allowN);
    }

    return res;
}

static EHaplotypeProblem CheckForHaplotypeProblems(const TBioseqHandleList& bioseqs)
{
    ITERATE (TBioseqHandleList, cur_bioseq, bioseqs) {
        TBioseqHandleList::const_iterator next_bioseq = cur_bioseq;
        for (++next_bioseq; next_bioseq != bioseqs.end(); ++next_bioseq) {
            // strict match check
            bool has_problem = !IsSeqEqualOrOverlap(*cur_bioseq, *next_bioseq, false);

            // non-strict match check
            if (has_problem) {
                has_problem = !IsSeqEqualOrOverlap(*cur_bioseq, *next_bioseq, true);
                return has_problem ? nonStrictMatchProblem : strictMatchProblem;
            }
        }
    }

    return noProblem;
}

static const string kHaplotypeReport = "Haplotype Problem Report";
static const string kHaplotypeStrictMatchProblem = "There [is] [n] haplotype problem[s] (strict match)";
static const string kHaplotypeNonStrictMatchProblem = "There [is] [n] haplotype problem[s] (loose match, allowing Ns to differ)";

static const string kStrictMatchDescr = "(strict match)";
static const string kNonStrictMatchDescr = "(allowing N to match any)";

static void AddHaplotypeProblems(CDiscrepancyContext& context, CReportNode& report, const string& subtype, const string& match_descr, const THaplotypeMap::const_iterator& bioseqs)
{
    string sub_subtype = "[n] sequences have organism " + bioseqs->first.first + " haplotype " + bioseqs->first.second + " but the sequences do not match " + match_descr;
    ITERATE (TBioseqHandleList, bioseq_h, bioseqs->second) {
        report[kHaplotypeReport][subtype][sub_subtype].Add(*(context.NewBioseqObj(bioseq_h->GetBioseqCore(), 0)), false);
    }
    report[kHaplotypeReport][subtype].Incr();
}


DISCREPANCY_SUMMARIZE(HAPLOTYPE_MISMATCH)
{
    if (m_Objs.empty()) {
        return;
    }

    THaplotypeMap haplotypes;
    CollectBioseqsByTaxnameAndHaplotype(context, m_Objs[kTaxnameAndHaplotype].GetObjects(), haplotypes);

    CReportNode report;
    ITERATE (THaplotypeMap, cur, haplotypes) {
        if (cur->second.size() > 1) {
            EHaplotypeProblem res = CheckForHaplotypeProblems(cur->second);
            if (res == strictMatchProblem || res == nonStrictMatchProblem) {
                AddHaplotypeProblems(context, report, kHaplotypeStrictMatchProblem, kStrictMatchDescr, cur);
            }
            if (res == nonStrictMatchProblem) {
                AddHaplotypeProblems(context, report, kHaplotypeNonStrictMatchProblem, kNonStrictMatchDescr, cur);
            }
        }
    }

    m_ReportItems = report.Export(*this)->GetSubitems();
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
        diffs.push_back("mitochondrial genetic codes differ");
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

    COrgName::TMod::const_iterator it_first,
        it_second;

    if (first_mod_set && second_mod_set) {
        it_first = first.GetMod().cbegin();
        it_second = second.GetMod().cbegin();

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

        if (it_first != end_first) {
            first_mod_set = false;
        }
        if (it_second != end_second) {
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

static const string kBioSource = "BioSource";


DISCREPANCY_CASE(INCONSISTENT_BIOSOURCE, CBioSource, eDisc, "Inconsistent BioSource")
{
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if (!bioseq->IsNa() || context.GetCurrentSeqdesc().Empty()) {
        return;
    }
    CSeqdesc* seqdesc = const_cast<CSeqdesc*>(context.GetCurrentSeqdesc().GetPointer());
    context.NewSeqdescObj(CConstRef<CSeqdesc>(seqdesc), context.GetCurrentBioseqLabel());    // this will cache the seqdesc name and allow using CConstRef<CBioseq>(0) in context.NewSeqdescObj()
    m_Objs[kBioSource].Add(*context.NewBioseqObj(bioseq, &context.GetSeqSummary(), eKeepRef, false, seqdesc), true);
}


typedef pair<const CBioseq*, const CSeqdesc*> TBioseqSeqdesc;
typedef list<pair<const CBioSource*, list<TBioseqSeqdesc>>> TItemsByBioSource;

static void GetItemsByBiosource(TReportObjectList& objs, TItemsByBioSource& items)
{
    NON_CONST_ITERATE (TReportObjectList, obj, objs) {
        CDiscrepancyObject* dobj = dynamic_cast<CDiscrepancyObject*>(obj->GetPointer());
        if (dobj) {
            const CBioseq* cur_bioseq = dobj->GetBioseq();
            const CSeqdesc* cur_seqdesc = dynamic_cast<const CSeqdesc*>(dobj->GetMoreInfo().GetPointer());
            const CBioSource& cur_biosrc = cur_seqdesc->GetSource();

            bool found = false;
            NON_CONST_ITERATE (TItemsByBioSource, item, items) {
                if (item->first->Equals(cur_biosrc)) {
                    found = true;
                    item->second.push_back(make_pair(cur_bioseq, cur_seqdesc));
                    break;
                }
            }

            if (!found) {
                items.push_back(make_pair(&cur_biosrc, list<TBioseqSeqdesc>()));
                items.back().second.push_back(make_pair(cur_bioseq, cur_seqdesc));
            }
        }
    }
}


static const string kInconsistentBiosources = "[n/2] inconsistent contig source[s]";

DISCREPANCY_SUMMARIZE(INCONSISTENT_BIOSOURCE)
{
    if (m_Objs.empty()) {
        return;
    }

    TItemsByBioSource items;
    GetItemsByBiosource(m_Objs[kBioSource].GetObjects(), items);

    if (items.size() > 1) {
        m_Objs.GetMap().erase(kBioSource);
        TItemsByBioSource::iterator first = items.begin(),
                                    second = first;
        ++second;
        TInconsistecyDescriptionList diffs;
        GetBiosourceDifferences(*first->first, *second->first, diffs);
        string diff_str = NStr::Join(diffs, ", ");

        string subtype = kInconsistentBiosources;
        if (!diff_str.empty()) {
            subtype += " (" + diff_str + ")";
        }

        size_t subcat_index = 0;
        static size_t MAX_NUM_LEN = 10;

        ITERATE (TItemsByBioSource, item, items) {
            string subcat_num = NStr::SizetToString(subcat_index);
            subcat_num = string(MAX_NUM_LEN - subcat_num.size(), '0') + subcat_num;
            string subcat = "[*" + subcat_num + "*][n/2] contig[s] [has] identical sources that do not match another contig source";
            ++subcat_index;
            ITERATE (list<TBioseqSeqdesc>, bioseq_desc, item->second) {
                m_Objs[subtype][subcat].Add(*context.NewSeqdescObj(CConstRef<CSeqdesc>(bioseq_desc->second), kEmptyStr), false).Ext();
                m_Objs[subtype][subcat].Add(*context.NewBioseqObj(CConstRef<CBioseq>(bioseq_desc->first), 0), false).Ext();
            }
        }
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


// TAX_LOOKUP_MISMATCH
static const string kTaxnameMismatch = "[n] tax name[s] [does] not match taxonomy lookup";
static const string kOgRefs = "OrgRef";


DISCREPANCY_CASE(TAX_LOOKUP_MISMATCH, CBioSource, eDisc, "Find Tax Lookup Mismatches")
{
    if (!obj.IsSetOrg()) {
        return;
    }
    m_Objs[kOgRefs].Add(*context.NewFeatOrDescObj(eKeepRef, false, const_cast<CBioSource*>(&obj)));
}

static void GetOrgRefs(TReportObjectList& objs, vector<CRef<COrg_ref>>& org_refs)

{
    NON_CONST_ITERATE (TReportObjectList, obj, objs) {

        CDiscrepancyObject* dobj = dynamic_cast<CDiscrepancyObject*>(obj->GetPointer());
        if (dobj) {

            const CBioSource* biosrc = dynamic_cast<const CBioSource*>(dobj->GetMoreInfo().GetPointer());
            const COrg_ref& org_ref = biosrc->GetOrg();
            CRef<COrg_ref> new_org_ref(new COrg_ref);
            new_org_ref->Assign(org_ref);
            org_refs.push_back(new_org_ref);
        }
    }
}


static const CDbtag* GetTaxonTag(const COrg_ref& org)
{
    const CDbtag* ret = nullptr;
    if (org.IsSetDb()) {
        ITERATE (COrg_ref::TDb, db, org.GetDb()) {
            if ((*db)->IsSetDb() && NStr::EqualNocase((*db)->GetDb(), "taxon")) {
                ret = *db;
                break;
            }
        }
    }

    return ret;
}


static bool IsOrgDiffers(const COrg_ref& first, const COrg_ref& second)
{
    bool first_set = first.IsSetTaxname(),
         second_set = second.IsSetTaxname();
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


static void GetMismatchOrMissingOrgRefReport(CDiscrepancyContext& context, CReportNode& objs_node, const string& subitem, bool is_mismatch)
{
    vector<CRef<COrg_ref>> org_refs;
    GetOrgRefs(objs_node[kOgRefs].GetObjects(), org_refs);

    if (!org_refs.empty()) {

        CRef<CTaxon3_reply> reply = GetOrgRefs(org_refs);
        if (reply) {

            vector<CRef<COrg_ref>>::const_iterator org_ref = org_refs.begin();

            TReportObjectList& objs = objs_node[kOgRefs].GetObjects();
            TReportObjectList::iterator obj_it = objs.begin();

            ITERATE (CTaxon3_reply::TReply, item, reply->GetReply()) {

                bool report = false;
                if (is_mismatch) {
                    report = (*item)->IsData() && IsOrgDiffers(**org_ref, (*item)->GetData().GetOrg());
                }
                else {
                    report = !(*item)->IsData() || (*item)->IsError();
                }

                if (report) {
                    CDiscrepancyObject* dobj = dynamic_cast<CDiscrepancyObject*>(obj_it->GetPointer());
                    CConstRef<CSeqdesc> desc(dynamic_cast<const CSeqdesc*>(dobj->GetObject().GetPointer()));
                    CConstRef<CSeq_feat> feat(dynamic_cast<const CSeq_feat*>(dobj->GetObject().GetPointer()));
                    if (desc.Empty()) {
                        objs_node[subitem].Add(*context.NewDiscObj(feat));
                    }
                    else {
                        objs_node[subitem].Add(*context.NewSeqdescObj(desc, kEmptyStr));
                    }
                }
                ++org_ref;
                ++obj_it;
            }
        }
    }
    objs_node.GetMap().erase(kOgRefs);
}


DISCREPANCY_SUMMARIZE(TAX_LOOKUP_MISMATCH)
{
    if (m_Objs.empty()) {
        return;
    }
    GetMismatchOrMissingOrgRefReport(context, m_Objs, kTaxnameMismatch, true);
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// TAX_LOOKUP_MISSING
static const string kTaxlookupMissing = "[n] tax name[s] [is] missing in taxonomy lookup";

DISCREPANCY_CASE(TAX_LOOKUP_MISSING, CBioSource, eDisc, "Find Missing Tax Lookup")
{
    if (!obj.IsSetOrg()) {
        return;
    }
    m_Objs[kOgRefs].Add(*context.NewFeatOrDescObj(eKeepRef, false, const_cast<CBioSource*>(&obj)));
}


DISCREPANCY_SUMMARIZE(TAX_LOOKUP_MISSING)
{
    if (m_Objs.empty()) {
        return;
    }

    GetMismatchOrMissingOrgRefReport(context, m_Objs, kTaxlookupMissing, false);
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// UNNECESSARY_ENVIRONMENTAL

DISCREPANCY_CASE(UNNECESSARY_ENVIRONMENTAL, CBioSource, eOncaller, "Unnecessary environmental qualifier present")
{
    if (!obj.IsSetSubtype()) {
        return;
    }
    bool found = false;
    ITERATE (CBioSource::TSubtype, subtype, obj.GetSubtype()) {
        if ((*subtype)->IsSetSubtype()) {
            CSubSource::TSubtype st = (*subtype)->GetSubtype();
            if (st == CSubSource::eSubtype_metagenomic) {
                return;
            }
            else if (st == CSubSource::eSubtype_other && NStr::FindNoCase((*subtype)->GetName(), "amplified with species-specific primers") != NPOS) {
                return;
            }
            else if (st == CSubSource::eSubtype_environmental_sample) {
                found = true;
            }
        }
    }
    if (!found) {
        return;
    }
    if (obj.IsSetOrg()) {
        if (obj.GetOrg().IsSetTaxname()) {
            const string& s = obj.GetOrg().GetTaxname();
            if (NStr::FindNoCase(s, "uncultured") != NPOS
                    || NStr::FindNoCase(s, "enrichment culture") != NPOS || NStr::FindNoCase(s, "metagenome") != NPOS
                    || NStr::FindNoCase(s, "environmental") != NPOS || NStr::FindNoCase(s, "unidentified") != NPOS) {
                return;
            }
        }
        if (obj.GetOrg().IsSetOrgname() && obj.GetOrg().GetOrgname().IsSetMod()) {
            ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
                if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_other
                        && (*it)->IsSetSubname() && NStr::FindNoCase((*it)->GetSubname(), "amplified with species-specific primers") != NPOS) {
                    return;
                }
            }
        }
    }
    m_Objs["[n] biosource[s] [has] unnecessary environmental qualifier"].Add(*context.NewFeatOrDescObj());
}


DISCREPANCY_SUMMARIZE(UNNECESSARY_ENVIRONMENTAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

// END_COLON_IN_COUNTRY

DISCREPANCY_CASE(END_COLON_IN_COUNTRY, CBioSource, eOncaller, "Country name end with colon")
{
    if (!obj.IsSetSubtype()) {
        return;
    }
    ITERATE (CBioSource::TSubtype, subtype, obj.GetSubtype()) {
        if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_country) {
            const string& s = (*subtype)->GetName();
            if (s.length() && s[s.length()-1] == ':') {
                m_Objs["[n] country source[s] end[S] with a colon."].Add(*context.NewFeatOrDescObj(eNoRef, true));
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
    ITERATE (CBioSource::TSubtype, subtype, src.GetSubtype()) {
        if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_country) {
            CSubSource& ss = const_cast<CSubSource&>(**subtype);
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
    TReportObjectList list = item->GetDetails();
    unsigned int n = AutofixBiosrc(list, scope, RemoveCountryColon);
    return CRef<CAutofixReport>(n ? new CAutofixReport("END_COLON_IN_COUNTRY: [n] country name[s] fixed", n) : 0);
}


// COUNTRY_COLON

DISCREPANCY_CASE(COUNTRY_COLON, CBioSource, eOncaller, "Country description should only have 1 colon")
{
    if (!obj.IsSetSubtype()) {
        return;
    }
    ITERATE (CBioSource::TSubtype, subtype, obj.GetSubtype()) {
        if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_country) {
            const string& s = (*subtype)->GetName();
            int count = 0;
            for (size_t i = 0; i < s.length(); i++) {
                if (s[i] == ':') {
                    count++;
                    if (count > 1) {
                        m_Objs["[n] country source[s] [has] more than 1 colon."].Add(*context.NewFeatOrDescObj(eNoRef, true));
                        break;
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
    ITERATE (CBioSource::TSubtype, subtype, src.GetSubtype()) {
        if ((*subtype)->IsSetSubtype() && (*subtype)->GetSubtype() == CSubSource::eSubtype_country) {
            CSubSource& ss = const_cast<CSubSource&>(**subtype);
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
    TReportObjectList list = item->GetDetails();
    unsigned int n = AutofixBiosrc(list, scope, ChangeCountryColonToComma);
    return CRef<CAutofixReport>(n ? new CAutofixReport("COUNTRY_COLON: [n] country name[s] fixed", n) : 0);
}


// HUMAN_HOST

DISCREPANCY_CASE(HUMAN_HOST, CBioSource, eDisc | eOncaller, "\'Human\' in host should be \'Homo sapiens\'")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().CanGetOrgname() || !obj.GetOrg().GetOrgname().CanGetMod()) {
        return;
    }
    ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*it)->CanGetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_nat_host && NStr::FindNoCase((*it)->GetSubname(), "human") != NPOS) {
            m_Objs["[n] organism[s] [has] \'human\' host qualifiers"].Add(*context.NewFeatOrDescObj(eNoRef, true));
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
    ITERATE (COrgName::TMod, it, src.GetOrg().GetOrgname().GetMod()) {
        if ((*it)->CanGetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_nat_host && NStr::FindNoCase((*it)->GetSubname(), "human") != NPOS) {
            COrgMod& om = const_cast<COrgMod&>(**it);
            NStr::ReplaceInPlace(om.SetSubname(), "human", "Homo sapiens");
            fixed = true;
        }
    }
    return fixed;
}


DISCREPANCY_AUTOFIX(HUMAN_HOST)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = AutofixBiosrc(list, scope, FixHumanHost);
    return CRef<CAutofixReport>(n ? new CAutofixReport("HUMAN_HOST: [n] host qualifier[s] fixed", n) : 0);
}


// CHECK_AUTHORITY

DISCREPANCY_CASE(CHECK_AUTHORITY, CBioSource, eDisc | eOncaller, "Authority and Taxname should match first two words")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().CanGetOrgname() || !obj.GetOrg().GetOrgname().CanGetMod() || !obj.GetOrg().CanGetTaxname() || !obj.GetOrg().GetTaxname().length()) {
        return;
    }
    string tax1, tax2;
    ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*it)->CanGetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_authority) {
            if (tax1.empty()) {
                list<CTempString> tmp;
                NStr::Split(obj.GetOrg().GetTaxname(), " ", tmp);
                list<CTempString>::iterator p = tmp.begin();
                if (p != tmp.end()) {
                    tax1 = *p;
                    p++;
                    if (p != tmp.end()) {
                        tax2 = *p;
                    }
                }
            }
            string aut1, aut2;
            list<CTempString> tmp;
            NStr::Split((*it)->GetSubname(), " ", tmp);
            list<CTempString>::iterator p = tmp.begin();
            if (p != tmp.end()) {
                aut1 = *p;
                p++;
                if (p != tmp.end()) {
                    aut2 = *p;
                }
            }
            if (aut1 != tax1 || aut2 != tax2) {
                m_Objs["[n] biosource[s] [has] taxname/authority conflict"].Add(*context.NewFeatOrDescObj());
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
        ITERATE (COrgName::TMod, it, bs.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->CanGetSubtype() && (*it)->GetSubtype() == qual) {
                return (*it)->GetSubname();
            }
        }
    }
    return kEmptyStr;
}


DISCREPANCY_CASE(TRINOMIAL_SHOULD_HAVE_QUALIFIER, CBioSource, eDisc | eOncaller | eSmart, "Trinomial sources should have corresponding qualifier")
{
    if (!obj.IsSetOrg() || !obj.GetOrg().CanGetTaxname() || !obj.GetOrg().GetTaxname().length() || NStr::FindNoCase(obj.GetOrg().GetTaxname(), " x ") != NPOS 
            || CDiscrepancyContext::HasLineage(obj, context.GetLineage(), "Viruses")) {
        return;
    }
    const string& taxname = obj.GetOrg().GetTaxname();
    for (size_t i = 0; i < srcqual_keywords_sz; i++) {
        size_t n = NStr::FindNoCase(taxname, srcqual_keywords[i].second);
        if (n != NPOS) {
            for (n+= srcqual_keywords[i].second.length(); n < taxname.length(); n++) {
                if (taxname[n] != ' ') {
                    break;
                }
            }
            if (n < taxname.length()) {
                string q = GetSrcQual(obj, srcqual_keywords[i].first);
                string s = taxname.substr(n, q.length());
                if (!q.length() || NStr::CompareNocase(s, q)) {
                    m_Objs["[n] trinomial source[s] lack[S] corresponding qualifier"].Add(*context.NewFeatOrDescObj());
                }
                break;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(TRINOMIAL_SHOULD_HAVE_QUALIFIER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE

static const string kAmplifiedPrimers = "[n] biosource[s] [has] \'amplified with species-specific primers\' note but no environmental-sample qualifier.";

DISCREPANCY_CASE(AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE, CBioSource, eOncaller, "Species-specific primers, no environmental sample")
{
    if (obj.HasSubtype(CSubSource::eSubtype_environmental_sample)) {
        return;
    }
    bool has_primer_note = false;
    if (obj.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, obj.GetSubtype()) {
            if ((*it)->GetSubtype() == CSubSource::eSubtype_other && NStr::FindNoCase((*it)->GetName(), "amplified with species-specific primers") != NPOS) {
                has_primer_note = true;
                break;
            }
        }
    }
    if (!has_primer_note && obj.IsSetOrg() && obj.GetOrg().CanGetOrgname() && obj.GetOrg().GetOrgname().CanGetMod()) {
        ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->CanGetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_other && (*it)->IsSetSubname() && NStr::FindNoCase((*it)->GetSubname(), "amplified with species-specific primers") != NPOS) {
                has_primer_note = true;
                break;
            }
        }
    }
    if (has_primer_note) {
        m_Objs[kAmplifiedPrimers].Add(*context.NewFeatOrDescObj(eNoRef, true));
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
    NON_CONST_ITERATE(CBioSource::TSubtype, s, src.SetSubtype()) {
        if ((*s)->GetSubtype() == CSubSource::eSubtype_other && (*s)->IsSetName()) {
            string orig = (*s)->GetName();
            NStr::ReplaceInPlace((*s)->SetName(), "[amplified with species-specific primers", "amplified with species-specific primers");
            NStr::ReplaceInPlace((*s)->SetName(), "amplified with species-specific primers]", "amplified with species-specific primers");
            if (!NStr::Equal(orig, (*s)->GetName())) {
                change = true;
                break;
            }
        }
    }

    return change;
}


DISCREPANCY_AUTOFIX(AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = AutofixBiosrc(list, scope, SetEnvSampleFixAmplifiedPrimers);
    return CRef<CAutofixReport>(n ? new CAutofixReport("AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE: Set environmental_sample, fixed amplified primers note for [n] source[s]", n) : 0);
}


// MISSING_PRIMER

DISCREPANCY_CASE(MISSING_PRIMER, CBioSource, eOncaller, "Missing values in primer set")
{
    if (!obj.CanGetPcr_primers() || !obj.GetPcr_primers().CanGet()) {
        return;
    }
    ITERATE (CPCRReactionSet::Tdata, pr, obj.GetPcr_primers().Get()) {
        const CPCRPrimerSet& fwdset = (*pr)->GetForward();
        const CPCRPrimerSet& revset = (*pr)->GetReverse();
        CPCRPrimerSet::Tdata::const_iterator fwd = fwdset.Get().begin();
        CPCRPrimerSet::Tdata::const_iterator rev = revset.Get().begin();
        while (fwd != fwdset.Get().end() && rev != revset.Get().end()) {
            if (((*fwd)->CanGetName() && !(*fwd)->GetName().Get().empty()) != ((*rev)->CanGetName() && !(*rev)->GetName().Get().empty())
                    || ((*fwd)->CanGetSeq() && !(*fwd)->GetSeq().Get().empty()) != ((*rev)->CanGetSeq() && !(*rev)->GetSeq().Get().empty())) {
                m_Objs["[n] biosource[s] [has] primer set[s] with missing values"].Add(*context.NewFeatOrDescObj());
                return;
            }
            fwd++;
            rev++;
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
    for (CPCRPrimerSet::Tdata::const_iterator it = a.begin(); it != a.end(); it++) {
        count_a++;
    }
    for (CPCRPrimerSet::Tdata::const_iterator jt = b.begin(); jt != b.end(); jt++) {
        count_b++;
    }
    if (count_a != count_b) {
        return false;
    }
    for (CPCRPrimerSet::Tdata::const_iterator it = a.begin(); it != a.end(); it++) {
        CPCRPrimerSet::Tdata::const_iterator jt = b.begin();
        for (; jt != b.end(); jt++) {
            if ((*it)->CanGetName() == (*jt)->CanGetName() && (*it)->CanGetSeq() == (*jt)->CanGetSeq()
                    && (!(*it)->CanGetName() || (*it)->GetName().Get() == (*jt)->GetName().Get())
                    && (!(*it)->CanGetSeq() || (*it)->GetSeq().Get() == (*jt)->GetSeq().Get())) {
                break;
            }
        }
        if (jt != b.end()) {
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


DISCREPANCY_CASE(DUPLICATE_PRIMER_SET, CBioSource, eOncaller, "Duplicate PCR primer pair")
{
    if (!obj.CanGetPcr_primers() || !obj.GetPcr_primers().CanGet()) {
        return;
    }
    const CPCRReactionSet::Tdata data = obj.GetPcr_primers().Get();
    for (CPCRReactionSet::Tdata::const_iterator it = data.begin(); it != data.end(); it++) {
        CPCRReactionSet::Tdata::const_iterator jt = it;
        for (jt++; jt != data.end(); jt++) {
            if (FindDuplicatePrimers(**it, **jt)) {
                m_Objs["[n] BioSource[s] [has] duplicate primer pairs."].Add(*context.NewFeatOrDescObj(eNoRef, true));
                return;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DUPLICATE_PRIMER_SET)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool FixDuplicatePrimerSet(CBioSource& src)
{
    if (!src.CanGetPcr_primers() || !src.GetPcr_primers().CanGet()) {
        return false;
    }
    bool fixed = false;
    // todo
    return fixed;
}


DISCREPANCY_AUTOFIX(DUPLICATE_PRIMER_SET)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = AutofixBiosrc(list, scope, FixDuplicatePrimerSet);
    return CRef<CAutofixReport>(n ? new CAutofixReport("DUPLICATE_PRIMER_SET: [n] PCR primer set[s] removed", n) : 0);
}


// METAGENOMIC

DISCREPANCY_CASE(METAGENOMIC, CBioSource, eDisc | eOncaller | eSmart, "Source has metagenomic qualifier")
{
    if (obj.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, obj.GetSubtype()) {
            if ((*it)->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                m_Objs["[n] biosource[s] [has] metagenomic qualifier"].Add(*context.NewFeatOrDescObj());
                return;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(METAGENOMIC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// METAGENOME_SOURCE

DISCREPANCY_CASE(METAGENOME_SOURCE, CBioSource, eDisc | eOncaller | eSmart, "Source has metagenome_source qualifier")
{
    if (obj.IsSetOrg() && obj.GetOrg().CanGetOrgname() && obj.GetOrg().GetOrgname().CanGetMod() && obj.GetOrg().IsSetTaxname() && !obj.GetOrg().GetTaxname().empty()) {
        ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->CanGetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_metagenome_source) {
                m_Objs["[n] biosource[s] [has] metagenome_source qualifier"].Add(*context.NewFeatOrDescObj());
                return;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(METAGENOME_SOURCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DUP_SRC_QUAL

DISCREPANCY_CASE(DUP_SRC_QUAL, CBioSource, eDisc | eOncaller | eSmart, "Each qualifier on a source should have different value")
{
    map<string, size_t> Map;
    string collected_by;
    string identified_by;
    if (obj.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, it, obj.GetSubtype()) {
            if ((*it)->CanGetName()) {
                const string& s = (*it)->GetName();
                if ((*it)->CanGetSubtype()) {
                    if ((*it)->GetSubtype() == CSubSource::eSubtype_collected_by) {
                        collected_by = s;
                    }
                    else if ((*it)->GetSubtype() == CSubSource::eSubtype_identified_by) {
                        identified_by = s;
                    }
                }
                if (!s.empty()) {
                    if (Map.find(s) != Map.end()) {
                        Map[s]++;
                    }
                    else {
                        Map[s] = 0;
                    }
                }
            }
        }
    }
    if (obj.IsSetOrg() && obj.GetOrg().CanGetOrgname() && obj.GetOrg().GetOrgname().CanGetMod()) {
        ITERATE (COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->IsSetSubname()) {
                const string& s = (*it)->GetSubname();
                if (!s.empty()) {
                    if (Map.find(s) != Map.end()) {
                        Map[s]++;
                    }
                    else {
                        Map[s] = 0;
                    }
                }
            }
        }
    }
    for (map<string, size_t>::const_iterator it = Map.begin(); it != Map.end(); it++) {
        if (it->second) {
            if (it->second == 1 && it->first == collected_by && collected_by == identified_by) {
                continue; // there is no error if collected_by equals to identified_by
            }
            string s = "[n] biosource[s] [has] repeating qualifier value \'";
            s += it->first;
            s += "\'";
            m_Objs[s].Add(*context.NewFeatOrDescObj());
        }
    }
}


DISCREPANCY_SUMMARIZE(DUP_SRC_QUAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_ALIAS(DUP_SRC_QUAL, DUP_SRC_QUAL_DATA)


// UNUSUAL_ITS
const string kUnusualITS = "[n] Bioseq[s] [has] unusual rRNA / ITS";

DISCREPANCY_CASE(UNUSUAL_ITS, CBioSource, eDisc | eOncaller, "Test Bioseqs for unusual rRNA / ITS")
{
    if (!context.HasLineage(obj, "", "Microsporidia")) {
        return;
    }

    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if (!bioseq || !bioseq->IsSetAnnot()) {
        return;
    }

    const CSeq_annot* annot = nullptr;
    ITERATE(CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
        if ((*annot_it)->IsFtable()) {
            annot = *annot_it;
            break;
        }
    }

    bool has_unusual = false;

    if (annot) {
        ITERATE(CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {
            if ((*feat)->IsSetComment() && (*feat)->IsSetData() && (*feat)->GetData().IsRna()) {
                const CRNA_ref& rna = (*feat)->GetData().GetRna();
                if (rna.IsSetType() && rna.GetType() == CRNA_ref::eType_miscRNA) {
                    if (NStr::StartsWith((*feat)->GetComment(), "contains", NStr::eNocase)) {
                        has_unusual = true;
                        break;
                    }
                }
            }
        }
    }

    if (has_unusual) {
        m_Objs[kUnusualITS].Add(*context.NewBioseqObj(bioseq, &context.GetSeqSummary()), false);
    }
}


DISCREPANCY_SUMMARIZE(UNUSUAL_ITS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
