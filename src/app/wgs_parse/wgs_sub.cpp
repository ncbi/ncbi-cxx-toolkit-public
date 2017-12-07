/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <serial/iterator.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_sub.hpp>

#include "wgs_sub.hpp"
#include "wgs_seqentryinfo.hpp"
#include "wgs_params.hpp"
#include "wgs_utils.hpp"
#include "wgs_asn.hpp"
#include "wgs_tax.hpp"
#include "wgs_med.hpp"

namespace wgsparse
{

template <typename R, typename T> R GetOrderValue(const CSeq_entry& entry)
{
    R ret;
    if (entry.IsSeq()) {
        ret = T::GetValue(entry.GetSeq());
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {

            for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
                ret = GetOrderValue<R, T>(*cur_entry);
                if (T::IsValuePresent(ret)) {
                    break;
                }
            }
        }
    }

    return ret;
}

static string GetLocalOrGeneralIdStr(const CSeq_id& id)
{
    return id.IsLocal() ? GetIdStr(id.GetLocal()) : GetIdStr(id.GetGeneral().GetTag());
}

static bool IsLocalOrGeneralId(const CSeq_id& id)
{
    return (id.IsGeneral() && id.GetGeneral().IsSetTag()) || id.IsLocal();
}

struct CSortByIdValue
{
    static string GetValue(const CBioseq& bioseq)
    {
        string ret;
        if (bioseq.IsNa() && bioseq.IsSetId()) {

            const CBioseq::TId& ids = bioseq.GetId();
            CBioseq::TId::const_iterator id = find_if(ids.begin(), ids.end(), [](const CRef<CSeq_id>& id){ return IsLocalOrGeneralId(*id); });

            if (id != ids.end()) {
                ret = GetLocalOrGeneralIdStr(**id);
            }
        }

        return ret;
    }

    static bool IsValuePresent(const string& val)
    {
        return !val.empty();
    }
};


static bool HasTextAccession(const CSeq_id& id)
{
    if (!id.IsGenbank() && !id.IsEmbl() && !id.IsDdbj() && !id.IsOther() && !id.IsTpd() && !id.IsTpe() && !id.IsTpg()) {
        return false;
    }
    return id.GetTextseq_Id() != nullptr && id.GetTextseq_Id()->IsSetAccession();
}

static string GetTextAccession(const CSeq_id& id)
{
    return id.GetTextseq_Id()->GetAccession();
}

struct CSortByAccessionValue
{
    static string GetValue(const CBioseq& bioseq)
    {
        string ret;
        if (bioseq.IsNa() && bioseq.IsSetId()) {

            const CBioseq::TId& ids = bioseq.GetId();
            CBioseq::TId::const_iterator id = find_if(ids.begin(), ids.end(), [](const CRef<CSeq_id>& id){ return HasTextAccession(*id); });

            if (id != ids.end()) {
                ret = GetTextAccession(**id);
            }
        }

        return ret;
    }

    static bool IsValuePresent(const string& val)
    {
        return !val.empty();
    }
};

struct CSortByLength
{
    static size_t GetValue(const CBioseq& bioseq)
    {
        size_t ret = 0;
        if (bioseq.IsNa() && bioseq.IsSetInst() && bioseq.GetInst().IsSetLength()) {

            ret = bioseq.GetInst().GetLength();
        }

        return ret;
    }

    static bool IsValuePresent(size_t val)
    {
        return val != 0;
    }
};


struct CSortedItem
{
    CRef<CSeq_entry> m_entry;
    size_t m_len;
    string m_accession;
};

static void GetOrderValueForEntry(CSortedItem& item, ESortOrder sort_order)
{
    switch (sort_order) {
        case eByAccession:
            item.m_accession = GetOrderValue<string, CSortByAccessionValue>(*item.m_entry);
            if (item.m_accession.empty()) {
                item.m_accession = "Unknown";
            }
            break;
        case eById:
            item.m_accession = GetOrderValue<string, CSortByIdValue>(*item.m_entry);
            if (item.m_accession.empty()) {
                item.m_accession = "Unknown";
            }
            break;
        case eSeqLenDesc:
        case eSeqLenAsc:
            item.m_len = GetOrderValue<size_t, CSortByLength>(*item.m_entry);
            break;
    }
}

static void SortSeqEntries(vector<CSortedItem>& items, ESortOrder sort_order)
{
    switch (sort_order) {
        case eByAccession:
        case eById:
            sort(items.begin(), items.end(), [](const CSortedItem& a, const CSortedItem& b){ return a.m_accession < b.m_accession; });
            break;
        case eSeqLenDesc:
            sort(items.begin(), items.end(), [](const CSortedItem& a, const CSortedItem& b){ return a.m_len > b.m_len; });
            break;
        case eSeqLenAsc:
            sort(items.begin(), items.end(), [](const CSortedItem& a, const CSortedItem& b){ return a.m_len < b.m_len; });
            break;
    }
}

static size_t ReversedSortSeqSubmit(list<CRef<CSeq_entry>>& entries, ESortOrder sort_order)
{
    size_t num_of_entries = entries.size(),
           cur = 0;

    vector<CSortedItem> items(num_of_entries);
    for (auto& entry : entries) {
        items[cur].m_entry = entry;
        GetOrderValueForEntry(items[cur], sort_order);
        ++cur;
    }

    SortSeqEntries(items, sort_order);
    entries.clear();
    for (auto& item : items) {
        entries.push_back(item.m_entry);
    }

    return entries.size();
}

static void RemoveDatesFromDescrs(CSeq_descr::Tdata& descrs, bool remove_creation, bool remove_update)
{
    for (auto descr = descrs.begin(); descr != descrs.end();) {

        if ((*descr)->IsCreate_date() && remove_creation ||
            (*descr)->IsUpdate_date() && remove_update) {
            descr = descrs.erase(descr);
        }
        else
            ++descr;
    }
}

static void RemoveDates(CSeq_entry& entry, bool remove_creation, bool remove_update)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {
        RemoveDatesFromDescrs(descrs->Set(), remove_creation, remove_update);
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveDates(*cur_entry, remove_creation, remove_update);
        }
    }
}

class CIsSamePub
{
public:
    CIsSamePub(const CPubdesc& pubdesc) : m_pubdesc(pubdesc) {}

    bool operator()(const CPubDescriptionInfo& pub_info) const
    {
        if (pub_info.m_pubdescr_lookup->Equals(m_pubdesc)) {
            return true;
        }

        for (auto& pub : pub_info.m_pubdescr_synonyms) {
            if (pub->Equals(m_pubdesc)) {
                return true;
            }
        }
        return false;
    }

private:
    const CPubdesc& m_pubdesc;
};

static bool IsCitSub(const CPubdesc& pub)
{
    if (pub.IsSetPub() && pub.GetPub().IsSet() && !pub.GetPub().Get().empty()) {

        return pub.GetPub().Get().front()->IsSub();
    }

    return false;
}

static void RemovePubs(CSeq_entry& entry, const list<CPubDescriptionInfo>& common_pubs, const CDate_std* date)
{
    if (common_pubs.empty()) {
        return;
    }

    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto& cur_descr = descrs->Set().begin(); cur_descr != descrs->Set().end();) {

            bool removed = false;
            if ((*cur_descr)->IsPub()) {

                CDate orig_date;
                CCit_sub* cit_sub = nullptr;
                if (date || GetParams().GetSource() != eNCBI) {

                    if (IsCitSub((*cur_descr)->GetPub())) {
                        cit_sub = &(*cur_descr)->SetPub().SetPub().Set().front()->SetSub();
                        if (cit_sub->IsSetDate()) {
                            orig_date.Assign(cit_sub->GetDate());
                        }

                        if (GetParams().GetSource() == eNCBI && date) {
                            cit_sub->SetDate().SetStd().Assign(*date);
                        }
                        else {
                            cit_sub->ResetDate();
                        }
                    }
                }

                if (find_if(common_pubs.begin(), common_pubs.end(), CIsSamePub((*cur_descr)->GetPub())) != common_pubs.end()) {
                    cur_descr = descrs->Set().erase(cur_descr);
                    removed = true;
                }
                else {

                    if (cit_sub) {
                        if (orig_date.Which() == CDate::e_not_set) {
                            cit_sub->ResetDate();
                        }
                        else {
                            cit_sub->SetDate().Assign(orig_date);
                        }
                    }
                }
            }
            if (!removed) {
                ++cur_descr;
            }
        }
    }
}

static bool ContainsLocals(const CBioseq::TId& ids)
{
    return find_if(ids.begin(), ids.end(), [](const CRef<CSeq_id>& id) { return id->IsLocal(); }) != ids.end();
}

static bool HasLocals(const CSeq_entry& entry)
{
    if (entry.IsSeq() && entry.GetSeq().IsSetId()) {
        return ContainsLocals(entry.GetSeq().GetId());
    }
    else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            if (HasLocals(*cur_entry)) {
                return true;
            }
        }
    }
    return false;
}

static void CollectObjectIds(const CSeq_entry& entry, map<string, string>& ids)
{
    if (entry.IsSeq()) {

        bool nuc = entry.GetSeq().IsNa();

        if (entry.GetSeq().IsSetId()) {
            for (auto& id : entry.GetSeq().GetId()) {
                if (IsLocalOrGeneralId(*id)) {
                    string id_str = GetLocalOrGeneralIdStr(*id),
                           dbname;

                    if (nuc && !GetParams().IsChromosomal()) {
                        dbname = GetParams().GetProjAccVerStr();
                    }
                    else {
                        dbname = GetParams().GetProjAccStr();
                    }

                    ids[id_str] = dbname;
                }
            }
        }
    }
    else if (entry.IsSet()) {

        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
                CollectObjectIds(*cur_entry, ids);
            }
        }
    }
}

static void FixDbNameInObject(CSerialObject& obj, const map<string, string>& ids)
{
    for (CTypeIterator<CSeq_id> id(obj); id; ++id) {
        if (id->IsGeneral()) {

            CDbtag& dbtag = id->SetGeneral();
            if (dbtag.IsSetDb() && dbtag.IsSetTag()) {

                string cur_id = GetIdStr(dbtag.GetTag());
                auto dbname = ids.find(cur_id);

                if (dbname != ids.end()) {
                    dbtag.SetDb(dbname->second);
                }
            }
        }
        else if (id->IsLocal()) {

            string cur_id = GetIdStr(id->GetLocal());
            auto dbname = ids.find(cur_id);

            if (dbname != ids.end()) {
                CDbtag& dbtag = id->SetGeneral();
                dbtag.SetDb(dbname->second);
                dbtag.SetTag().SetStr(cur_id);
            }
        }
    }
}

static void FixDbName(CSeq_entry& entry)
{
    map<string, string> ids;
    CollectObjectIds(entry, ids);

    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs) {
        FixDbNameInObject(*descrs, ids);
    }

    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {

        for (auto& annot : *annots) {
            FixDbNameInObject(*annot, ids);
        }
    }

    if (entry.IsSeq() && entry.GetSeq().IsSetInst()) {
        FixDbNameInObject(entry.SetSeq().SetInst(), ids);
    }
}

static void FixOrigProtTransValue(string& val)
{
    size_t pos = val.find('|');
    string no_prefix_val = pos == string::npos ? val : val.substr(pos + 1);

    val = "gnl|" + GetParams().GetProjPrefix() + '|' + GetParams().GetIdPrefix() + '|' + no_prefix_val;
}

static void FixOrigProtTransQuals(CSeq_annot::C_Data::TFtable& ftable)
{
    for (auto& feat : ftable) {

        if (feat->IsSetQual()) {

            for (auto& qual : feat->SetQual()) {
                if (qual->IsSetQual() &&
                    (qual->GetQual() == "orig_protein_id" || qual->GetQual() == "orig_transcript_id")) {
                    FixOrigProtTransValue(qual->SetVal());
                }
            }
        }
    }
}

static void FixOrigProtTransIds(CSeq_entry& entry)
{
    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {
        for (auto& annot : *annots) {
            if (annot->IsFtable()) {
                FixOrigProtTransQuals(annot->SetData().SetFtable());
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto cur_entry : entry.GetSet().GetSeq_set()) {
            FixOrigProtTransIds(*cur_entry);
        }
    }
}

static void CollectAccGenid()
{
}

static CRef<CSeq_id> CreateNewAccession(int num)
{
    CRef<CSeq_id> ret;

    CRef<CTextseq_id> text_id(new CTextseq_id);
    int ver = GetParams().GetAssemblyVersion();
    text_id->SetAccession(GetParams().GetIdPrefix() + ToStringLeadZeroes(ver, 2) + ToStringLeadZeroes(num, GetMaxAccessionLen(num)));

    auto set_fun = FindSetTextSeqIdFunc(GetParams().GetIdChoice());
    _ASSERT(set_fun != nullptr && "There should be a valid SetTextId function. Validate the ID choice.");

    if (set_fun == nullptr) {
        return ret;
    }

    ret.Reset(new CSeq_id);
    (ret->*set_fun)(*text_id);

    return ret;
}

static void RemovePreviousAccession(const string& new_acc, CBioseq::TId& ids)
{
    for (auto id = ids.begin(); id != ids.end(); ++id) {
        if (HasTextAccession(**id)) {

            const CTextseq_id* text_id = (*id)->GetTextseq_Id();
            _ASSERT(text_id != nullptr);

            if (text_id->GetAccession() != new_acc) {
                ERR_POST_EX(0, 0, Info << "Input Seq-entry already has accession \"" << text_id->GetAccession() << "\". Replaced with \"" << new_acc << "\".");
            }

            ids.erase(id);
            break;
        }
    }
}

static void AssignNucAccession(CSeq_entry& entry, int& next_id)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {

        if (GetParams().IsAccessionAssigned()) {

            if (!GetParams().IsTest()) {
                // TODO
                CollectAccGenid();
            }
            return;
        }

        // TODO all processing linked with sort parameters

        CRef<CSeq_id> new_id = CreateNewAccession(next_id);

        if (new_id.NotEmpty()) {

            const string& new_acc = new_id->GetTextseq_Id()->GetAccession();
            ERR_POST_EX(0, 0, Info << "Assigned nucleotide accession \"" << new_acc << "\".");

            RemovePreviousAccession(new_acc, entry.SetSeq().SetId());
            entry.SetSeq().SetId().push_front(new_id);
        }

        if (!GetParams().IsTest()) {
            // TODO
            CollectAccGenid();
        }

        if (GetParams().GetUpdateMode() != eUpdateScaffoldsNew || GetParams().IsScaffoldTestMode()) {
            return;
        }

        // TODO
    }
    
    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
                AssignNucAccession(*cur_entry, next_id);
            }
        }
    }
}

static void FixTechMolInfo(CMolInfo& mol_info, CSeq_inst::EMol& mol)
{
    if (GetParams().IsTsa()) {
        if (GetParams().GetFixTech() & eFixMolBiomol) {

            mol_info.SetTech(CMolInfo::eTech_tsa);
            mol_info.SetBiomol(CMolInfo::eBiomol_transcribed_RNA);

            mol = CSeq_inst::eMol_rna;
        }

        if (GetParams().GetFixTech() & eFixBiomolMRNA) {
            mol_info.SetBiomol(CMolInfo::eBiomol_mRNA);
        }
        else if (GetParams().GetFixTech() & eFixBiomolRRNA) {
            mol_info.SetBiomol(CMolInfo::eBiomol_rRNA);
        }
        else if (GetParams().GetFixTech() & eFixBiomolNCRNA) {
            mol_info.SetBiomol(CMolInfo::eBiomol_ncRNA);
        }
    }
    else {
        if (GetParams().GetFixTech() & eFixMolBiomol) {

            mol_info.SetTech(GetParams().IsTls() ? CMolInfo::eTech_targeted : CMolInfo::eTech_wgs);
            mol_info.SetBiomol(CMolInfo::eBiomol_genomic);

            mol = CSeq_inst::eMol_dna;
        }

        if (GetParams().IsWgs()) {

            if (GetParams().GetFixTech() & eFixBiomolCRNA) {
                mol_info.SetBiomol(CMolInfo::eBiomol_cRNA);
            }

            if (GetParams().GetFixTech() & eFixInstMolRNA) {
                mol = CSeq_inst::eMol_rna;
            }
        }
    }
}

static void FixTech(CSeq_entry& entry)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {
        
        CRef<CSeqdesc> mol_info_descr;
        CSeq_descr& descrs = entry.SetSeq().SetDescr();
        
        auto mol_info_it = find_if(descrs.Set().begin(), descrs.Set().end(), [](CRef<CSeqdesc>& descr) { return descr->IsMolinfo(); });
        if (mol_info_it != descrs.Get().end()) {
            mol_info_descr = *mol_info_it;
        }
        else {
            mol_info_descr.Reset(new CSeqdesc);
            descrs.Set().push_back(mol_info_descr);
        }

        FixTechMolInfo(mol_info_descr->SetMolinfo(), entry.SetSeq().SetInst().SetMol());
    }
    
    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
                FixTech(*cur_entry);
            }
        }
    }
}

static bool FixBioSources(CSeq_entry& entry, const CMasterInfo& master_info)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        auto source_it = find_if(descrs->Set().begin(), descrs->Set().end(), [](CRef<CSeqdesc>& descr) { return descr->IsSource(); });

        if (source_it != descrs->Set().end() && !PerformTaxLookup((*source_it)->SetSource(), master_info.m_org_refs, GetParams().IsTaxonomyLookup())) {
            return false;
        }
    }

    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {

        auto feat_table_it = find_if(annots->begin(), annots->end(), [](CRef<CSeq_annot>& annot) { return annot->IsFtable(); });

        if (feat_table_it != annots->end()) {

            auto feat_table = (*feat_table_it)->SetData().SetFtable();
            auto feat_source_it = find_if(feat_table.begin(), feat_table.end(), [](CRef<CSeq_feat>& feat) { return feat->IsSetData() && feat->GetData().IsBiosrc(); });

            if (feat_source_it != feat_table.end() && PerformTaxLookup((*feat_source_it)->SetData().SetBiosrc(), master_info.m_org_refs, GetParams().IsTaxonomyLookup())) {
                return false;
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {

            if (!FixBioSources(*cur_entry, master_info)) {
                return false;
            }
        }
    }

    return true;
}

bool ParseSubmissions(CMasterInfo& master_info)
{
    const list<string>& files = GetParams().GetInputFiles();

    bool ret = true;
    int next_id = 0;

    for (auto& file : files) {
        CNcbiIfstream in(file);

        if (!in) {
            ERR_POST_EX(0, 0, "Failed to open submission \"" << file << "\" for reading. Cannot proceed.");
            ret = false;
            break;
        }

        EInputType input_type = eSeqSubmit;
        GetInputTypeFromFile(in, input_type);

        bool first = true;
        while (in && !in.eof()) {
            CRef<CSeq_submit> seq_submit = GetSeqSubmit(in, input_type);
            if (seq_submit.Empty()) {

                if (first) {
                    ERR_POST_EX(0, 0, "Failed to read " << GetSeqSubmitTypeName(input_type) << " from file \"" << file << "\". Cannot proceed.");
                    ret = false;
                }
                break;
            }

            first = false;
            FixSeqSubmit(seq_submit, master_info.m_accession_ver, false);

            ESortOrder sort_order = GetParams().GetSortOrder();
            if (sort_order != eUnsorted) {
                if (GetParams().IsAccessionAssigned()) {
                    sort_order = eByAccession;
                }
                ReversedSortSeqSubmit(seq_submit->SetData().SetEntrys(), sort_order);
            }

            // TODO WGSReplaceSubmissionDate

            // TODO deal with cit_sub

            if (seq_submit->IsSetData() && seq_submit->GetData().IsEntrys()) {

                for (auto& entry : seq_submit->SetData().SetEntrys()) {
                    
                    if (master_info.m_creation_date_present || master_info.m_update_date_present) {
                        RemoveDates(*entry, master_info.m_creation_date_present, master_info.m_update_date_present);
                    }

                    /* TODO if (kwds != FALSE)
                        SeqEntryExplore(sep, widp->keywords, WGSRemoveKeywords);
                    if (widp->tpa_keyword != NULL)
                        SeqEntryExplore(sep, widp->tpa_keyword,
                        WGSPropagateTPAKeyword);
                    if (widp->prot_gbblock != FALSE)
                        SeqEntryExplore(sep, NULL, WGSCleanupProtGbblock); */

                    if (GetParams().GetUpdateMode() != eUpdatePartial) {
                        /* TODO if (widp->comcoms != NULL)
                            SeqEntryExplore(sep, widp->comcoms, WGSRemoveComments);
                        if (widp->wsccp != NULL && widp->wsccp->com != NULL)
                            SeqEntryExplore(sep, widp->wsccp,
                            WGSRemoveStrComments);*/

                        if (!master_info.m_common_pubs.empty()) {
                            
                            RemovePubs(*entry, master_info.m_common_pubs, nullptr);
                        }
                    }

                    if (GetParams().IsReplaceDBName() || HasLocals(*entry)) {
                        FixDbName(*entry);
                        FixOrigProtTransIds(*entry);
                    }

                    if (!GetParams().IsVDBMode()) {
                        AssignNucAccession(*entry, next_id);
                    }

                    if (GetParams().IsUpdateScaffoldsMode()) {
                        // TODO
                    }

                    if (GetParams().IsForcedGencode()) {
                        // TODO
                    }

                    if (GetParams().GetFixTech() && (GetParams().IsTsa() || (GetParams().GetFixTech() | eFixMolBiomol))) {
                        FixTech(*entry);
                    }

                    // TODO gaps

                    if (master_info.m_reject) {
                        break;
                    }

                    bool lookup_succeed = FixBioSources(*entry, master_info);

                    // TODO dblink etc

                    if (!lookup_succeed) {
                        ERR_POST_EX(0, 0, Fatal << "Taxonomy lookup failed on submission \"" << file << "\". Cannot proceed.");
                        break;
                    }

                    if (GetParams().IsMedlineLookup() && !PerformMedlineLookup(*entry)) {
                        break;
                    }

                    /*
                    if(widp->strip_authors != FALSE)
                    SeqEntryExplore(sep, NULL, WGSStripAuthors);
                    if(widp->new_titles != NULL)
                    {
                    wtp.title = widp->new_titles;
                    if(wtp.organism != NULL)
                    MemFree(wtp.organism);
                    wtp.organism = NULL;
                    if(wtp.chro != NULL)
                    MemFree(wtp.chro);
                    wtp.chro = NULL;
                    SeqEntryExplore(sep, &wtp, WGSRepTitles);
                    }
                    */


                }
            }
        }
    }

    return ret;
}

}