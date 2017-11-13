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

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_inst.hpp>

#include "wgs_sub.hpp"
#include "wgs_seqentryinfo.hpp"
#include "wgs_params.hpp"
#include "wgs_utils.hpp"
#include "wgs_asn.hpp"

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
    for (auto& entry: entries) {
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

bool ParseSubmissions(CMasterInfo& master_info)
{
    const list<string>& files = GetParams().GetInputFiles();

    bool ret = true;

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

                }
            }
        }
    }

    return ret;
}

}