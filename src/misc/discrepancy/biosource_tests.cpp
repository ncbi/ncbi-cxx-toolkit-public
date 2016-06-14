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
#include <objmgr/seqdesc_ci.hpp>

#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(biosource_tests);


// MAP_CHROMOSOME_CONFLICT

const string kMapChromosomeConflict = "[n] source[s] on eukaryotic sequence[s] [has] map but not chromosome";


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MAP_CHROMOSOME_CONFLICT, CSeqdesc, eDisc | eOncaller, "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSource() || !context.IsEukaryotic() || !obj.GetSource().IsSetSubtype()) {
        return;
    }

    bool has_map = false;
    bool has_chromosome = false;

    ITERATE(CBioSource::TSubtype, it, obj.GetSource().GetSubtype()) {
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
        m_Objs[kMapChromosomeConflict].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj)),
            false).Fatal();
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MAP_CHROMOSOME_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
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
    size_t pos = NStr::Find(taxname, "/");
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
        ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
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
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(INFLUENZA_DATE_MISMATCH, CBioSource, eOncaller, "Influenza Strain/Collection Date Mismatch")
//  ----------------------------------------------------------------------------
{
    if (DoInfluenzaStrainAndCollectionDateMisMatch(obj)) {
        if (context.GetCurrentSeqdesc() != NULL) {
            m_Objs[kInfluenzaDateMismatch].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false).Fatal();
        } else if (context.GetCurrentSeq_feat() != NULL) {
            m_Objs[kInfluenzaDateMismatch].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false).Fatal();
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(INFLUENZA_DATE_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// UNCULTURED_NOTES

static bool HasUnculturedNotes(const CBioSource& src)
{
    if (!src.IsSetSubtype()) {
        return false;
    }
    ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
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

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(UNCULTURED_NOTES, CBioSource, eOncaller, "Uncultured Notes")
//  ----------------------------------------------------------------------------
{
    if (HasUnculturedNotes(obj)) {
        if (context.GetCurrentSeqdesc() != NULL) {
            m_Objs[kUnculturedNotes].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false).Fatal();
        } else if (context.GetCurrentSeq_feat() != NULL) {
            m_Objs[kUnculturedNotes].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false).Fatal();
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(UNCULTURED_NOTES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_VIRAL_QUALS

const string kMissingViralQualsTop = "[n] virus organism[s] [is] missing required qualifiers";
const string kMissingViralQualsCollectionDate = "[n] virus organism[s] [is] missing suggested qualifier collection date";
const string kMissingViralQualsCountry = "[n] virus organism[s] [is] missing suggested qualifier country";
const string kMissingViralQualsSpecificHost = "[n] virus organism[s] [is] missing suggested qualifier specific-host";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_VIRAL_QUALS, CBioSource, eOncaller, "Viruses should specify collection-date, country, and specific-host")
//  ----------------------------------------------------------------------------
{
    if (!CDiscrepancyContext::HasLineage(obj, context.GetLineage(), "Viruses")) {
        return;
    }
    bool has_collection_date = false;
    bool has_country = false;
    bool has_specific_host = false;
    if (obj.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, obj.GetSubtype()) {
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
        ITERATE(COrgName::TMod, it, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_nat_host) {
                has_specific_host = true;
            }
        }
    }
    if (!has_collection_date || !has_country || !has_specific_host) {
        if (context.GetCurrentSeqdesc() != NULL) {
            if (!has_collection_date) {
                m_Objs[kMissingViralQualsTop][kMissingViralQualsCollectionDate].Ext().Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false);
            }
            if (!has_country) {
                m_Objs[kMissingViralQualsTop][kMissingViralQualsCountry].Ext().Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false);
            }
            if (!has_specific_host) {
                m_Objs[kMissingViralQualsTop][kMissingViralQualsSpecificHost].Ext().Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false);
            }
        } else if (context.GetCurrentSeq_feat() != NULL) {
            if (!has_collection_date) {
                m_Objs[kMissingViralQualsTop][kMissingViralQualsCollectionDate].Ext().Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
            }
            if (!has_country) {
                m_Objs[kMissingViralQualsTop][kMissingViralQualsCountry].Ext().Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
            }
            if (!has_specific_host) {
                m_Objs[kMissingViralQualsTop][kMissingViralQualsSpecificHost].Ext().Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_VIRAL_QUALS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ATCC_CULTURE_CONFLICT
bool HasCultureCollectionForATCCStrain(const COrgName::TMod& mods, const string& strain)
{
    if (NStr::IsBlank(strain)) {
        return true;
    }

    bool found = false;

    ITERATE(COrgName::TMod, m, mods) {
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

    ITERATE(COrgName::TMod, m, mods) {
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


void AddObjSource(CReportNode& objs, CDiscrepancyContext& context, const string& category)
{
    if (context.GetCurrentSeqdesc() != NULL) {
        objs[category].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false);
    } else if (context.GetCurrentSeq_feat() != NULL) {
        objs[category].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
    }
}


const string kATCCCultureConflict = "[n] biosource[s] [has] conflicting ATCC strain and culture collection values";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(ATCC_CULTURE_CONFLICT, CBioSource, eOncaller|eDisc, "ATCC strain should also appear in culture collection")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }

    bool report = false;

    ITERATE(COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
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
        AddObjSource(m_Objs, context, kATCCCultureConflict);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(ATCC_CULTURE_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool SetCultureCollectionFromStrain(CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetOrgMod() || !src.GetOrg().GetOrgname().IsSetMod()) {
        return false;
    }

    vector<string> add;
    ITERATE(COrgName::TMod, m, src.GetOrg().GetOrgname().GetMod()) {
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
        ITERATE(vector<string>, s, add) {
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
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && sf->IsSetData() && sf->GetData().IsBiosrc()) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            if (SetCultureCollectionFromStrain(new_feat->SetData().SetBiosrc())) {
                CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
                feh.Replace(*new_feat);
                n++;
            }
        } else {
            const CSeqdesc* csd = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
            if (csd && csd->IsSource()) {
                CSeqdesc* sd = const_cast<CSeqdesc*>(csd);
                if (SetCultureCollectionFromStrain(sd->SetSource())) {
                    n++;
                }
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("ATCC_CULTURE_CONFLICT: Set culture collection for [n] source[s]", n) : 0);
}

// BACTERIA_MISSING_STRAIN
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BACTERIA_MISSING_STRAIN, CBioSource, eOncaller | eDisc, "Missing strain on bacterial 'Genus sp. strain'")
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
        ITERATE(COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
            if ((*m)->IsSetSubtype() &&
                (*m)->GetSubtype() == COrgMod::eSubtype_strain &&
                (*m)->IsSetSubname() &&
                NStr::Equal((*m)->GetSubname(), cmp)) {
                found = true;
            }
        }
    }
    if (!found) {
        AddObjSource(m_Objs, context, "[n] bacterial biosource[s] [has] taxname 'Genus sp. strain' but no strain");
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BACTERIA_MISSING_STRAIN)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
          return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
