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


void AddObjSource(CReportNode& objs, CDiscrepancyContext& context, const string& category, bool keepref = false)
{
    if (context.GetCurrentSeqdesc() != NULL) {
        if (keepref) {
            objs[category].Add(*context.NewDiscObj(context.GetCurrentSeqdesc(), eKeepRef), false);
        } else {
            objs[category].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false);
        }
    } else if (context.GetCurrentSeq_feat() != NULL) {
        if (keepref) {
            objs[category].Add(*context.NewDiscObj(context.GetCurrentSeq_feat(), eKeepRef), false);
        } else {
            objs[category].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false);
        }
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


// BACTERIA_SHOULD_NOT_HAVE_ISOLATE
const string kAmplifiedWithSpeciesSpecificPrimers = "amplified with species-specific primers";
bool HasAmplifiedWithSpeciesSpecificPrimerNote(const CBioSource& src)
{
    if (src.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, s, src.GetSubtype()) {
            if ((*s)->IsSetSubtype() && (*s)->GetSubtype() == CSubSource::eSubtype_other &&
                (*s)->IsSetName() && NStr::Equal((*s)->GetName(), kAmplifiedWithSpeciesSpecificPrimers)) {
                return true;
            }
        }
    }

    if (src.IsSetOrg() && src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE(COrgName::TMod, m, src.GetOrg().GetOrgname().GetMod()) {
            if ((*m)->IsSetSubtype() && (*m)->GetSubtype() == CSubSource::eSubtype_other &&
                (*m)->IsSetSubname() && NStr::Equal((*m)->GetSubname(), kAmplifiedWithSpeciesSpecificPrimers)) {
                return true;
            }
        }
    }

    return false;
}

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BACTERIA_SHOULD_NOT_HAVE_ISOLATE, CBioSource, eOncaller, "Bacterial sources should not have isolate")
//  ----------------------------------------------------------------------------
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
        ITERATE(COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
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
        AddObjSource(m_Objs, context, "[n] bacterial biosource[s] [has] isolate");
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BACTERIA_SHOULD_NOT_HAVE_ISOLATE)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MULTISRC

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MULTISRC, CBioSource, eOncaller | eDisc, "Comma or semicolon appears in strain or isolate")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }

    bool report = false;
    ITERATE(COrgName::TMod, m, obj.GetOrg().GetOrgname().GetMod()) {
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
        AddObjSource(m_Objs, context, "[n] organism[s] [has] comma or semicolon in strain or isolate");
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MULTISRC)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
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
    ITERATE(CBioseq::TAnnot, annot_it, bioseq->GetAnnot()) {
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
        ITERATE(CSeq_annot::TData::TFtable, feat, annot->GetData().GetFtable()) {

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
    if (m_Objs.empty()) {
        return;
    }
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

        ITERATE(CBioSource::TSubtype, subtype_it, biosource.GetSubtype())
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

            ITERATE(COrgName::TMod, mod_it, biosource.GetOrg().GetOrgname().GetMod()) {

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
        AddObjSource(m_Objs, context, kMissingRequiredClone);
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(REQUIRED_CLONE)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
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
DISCREPANCY_CASE(STRAIN_TAXNAME_MISMATCH, CBioSource, eDisc|eOncaller, "BioSources with the same strain should have the same taxname")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    ITERATE(COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_strain &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            AddObjSource(m_Objs, context, "[n] biosource[s] have strain " + (*om)->GetSubname() + " but do not have the same taxnames", true);
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
        NON_CONST_ITERATE(CReportNode::TNodeMap, it, m_Objs.GetMap()) {
            num_biosources += it->second->GetObjects().size();
        }
        string new_label = NStr::NumericToString(num_biosources) +
            " BioSources have " + qual_name + "/taxname conflicts";

        NON_CONST_ITERATE(CReportNode::TNodeMap, it, m_Objs.GetMap()) {
            NON_CONST_ITERATE(TReportObjectList, q, it->second->GetObjects()) {
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
DISCREPANCY_CASE(SPECVOUCHER_TAXNAME_MISMATCH, CBioSource, eOncaller, "BioSources with the same specimen voucher should have the same taxname")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetOrg() || !obj.GetOrg().IsSetOrgname() || !obj.GetOrg().GetOrgname().IsSetMod()) {
        return;
    }
    ITERATE(COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_specimen_voucher &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            AddObjSource(m_Objs, context, "[n] biosource[s] have specimen voucher " + (*om)->GetSubname() + " but do not have the same taxnames", true);
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
    ITERATE(COrgName::TMod, om, obj.GetOrg().GetOrgname().GetMod()) {
        if ((*om)->IsSetSubtype() && (*om)->GetSubtype() == COrgMod::eSubtype_culture_collection &&
            (*om)->IsSetSubname() && !NStr::IsBlank((*om)->GetSubname())) {
            AddObjSource(m_Objs, context, "[n] biosource[s] have culture collection " + (*om)->GetSubname() + " but do not have the same taxnames", true);
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
