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
* Authors: Sergiy Gotvyanskyy
*
* File Description:
*   Fast, indexed accessible five column feature table reader
*
*/
#include <ncbi_pch.hpp>
#include "annot_match.hpp"

#include <objtools/huge_asn/huge_file.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <mutex>
#include <util/memory_streambuf.hpp>
#include <objtools/readers/readfeat.hpp>


BEGIN_NCBI_SCOPE

using namespace objects;

CRef<CSeq_id> IIndexedFeatureReader::GetAnnotId(const CSeq_annot& annot)
{
    CRef<CSeq_id> pAnnotId;
    if (annot.IsSetId())
    {
        pAnnotId.Reset(new CSeq_id());
        const CAnnot_id& firstId = *(annot.GetId().front());
        if (firstId.IsLocal()) {
            pAnnotId->SetLocal().Assign(firstId.GetLocal());
        }
        else if (firstId.IsGeneral())
        {
            pAnnotId->SetGeneral().Assign(firstId.GetGeneral());
        }
        else {
            return pAnnotId;
        }
    }
    else if (!annot.GetData().GetFtable().empty())
    {
        // get a reference to CSeq_id instance, we'd need to update it recently
        // 5 column feature reader has a single shared instance for all features
        // update one at once would change all the features
        pAnnotId.Reset(const_cast<CSeq_id*>(annot.GetData().GetFtable().front()->GetLocation().GetId()));
    }

    return pAnnotId;
}

void CWholeFileAnnotation::Open(std::unique_ptr<TFile> file)
{
    CMemory_Streambuf istrbuf(file->m_memory, file->m_filesize);
    std::istream istr(&istrbuf);

    auto lr = ILineReader::New(istr);
    CRef<CSeq_annot> annot;
    while(!lr->AtEOF() && (annot = CFeature_table_reader::ReadSequinFeatureTable(*lr, m_reader_flags, m_logger, nullptr, m_seqid_prefix))) {
        if (annot && annot->IsFtable() && annot->GetData().GetFtable().size() > 0) {
            AddAnnot(annot);
        }
    }
}

bool CWholeFileAnnotation::s_HasPrefixMatch(
    const string& idString,
    map<string, TAnnotMap::iterator>& matchMap)
{
    matchMap.clear();
    auto it = m_AnnotMap.lower_bound(idString);
    while (it != m_AnnotMap.end() && NStr::StartsWith(it->first, idString)) {
        matchMap.emplace(it->first, it);
        ++it;
    }
    return !matchMap.empty();
}

bool CWholeFileAnnotation::x_HasMatch(
    bool matchVersions,
    const string& idString,
    list<CRef<CSeq_annot>>& annots)
{
    if (matchVersions) {
        return x_HasExactMatch(idString, annots);
    }

    bool hasMatch = false;
    map<string, TAnnotMap::iterator> matchMap;
    shared_lock<shared_mutex> sLock{m_Mutex};
    if (!s_HasPrefixMatch(idString, matchMap)) {
        return false;
    }
    sLock.unlock();
    {
        unique_lock<shared_mutex> uLock{m_Mutex};
        for (auto match : matchMap) {
            const auto& annotId = match.first;
            auto it = match.second;
            if (m_MatchedAnnots.insert(annotId).second) {
                hasMatch = true;
                annots.splice(annots.end(), it->second);
                m_AnnotMap.erase(it);
            }
        }
    }

    return hasMatch;
}

bool CWholeFileAnnotation::x_HasExactMatch(
    const string& idString,
    list<CRef<CSeq_annot>>& annots)
{
    shared_lock<shared_mutex> sLock{m_Mutex};
    auto it = m_AnnotMap.find(idString);
    if (it == m_AnnotMap.end()) {
        return false;
    }
    string annotId = it->first;
    sLock.unlock();

    {
        unique_lock<shared_mutex> uLock{m_Mutex};
        if (m_MatchedAnnots.insert(annotId).second) {
            annots = move(it->second);
            m_AnnotMap.erase(it);
            return true;
        }
    }

    return false;
}

void CWholeFileAnnotation::Init(const std::string& genome_center_id, long reader_flags, TLogger* logger)
{
    m_reader_flags = reader_flags;
    m_genome_center_id = genome_center_id;
    m_logger = logger;
    NStr::ToLower(m_genome_center_id);

    if (genome_center_id.empty())
        m_seqid_prefix.clear();
    else {
        m_seqid_prefix = "gnl|" + genome_center_id + "|";
    }
}

void CWholeFileAnnotation::AddAnnots(TAnnots& annots)
{
    for (auto annot: annots) {
        AddAnnot(annot);
    }
    //std::cerr << "Loaded annots: " << m_AnnotMap.size() << "\n";
}

std::list<CRef<CSeq_annot>> CWholeFileAnnotation::GetAndUseAnnot(CRef<CSeq_id> pSeqId)
{
    list<CRef<CSeq_annot>> annots;
    bool hasMatch = false;

    bool matchVersions = (pSeqId->GetTextseq_Id() == nullptr);
    auto idString = pSeqId->GetSeqIdString();
    NStr::ToLower(idString);

    hasMatch = x_HasMatch(matchVersions, idString, annots);

    if (!hasMatch &&
        pSeqId->IsGeneral() &&
        pSeqId->GetGeneral().IsSetDb() &&
        NStr::EqualNocase(pSeqId->GetGeneral().GetDb(), m_genome_center_id) &&
        pSeqId->GetGeneral().IsSetTag() && pSeqId->GetGeneral().GetTag().IsStr()) {
        matchVersions = true;
        idString = pSeqId->GetGeneral().GetTag().GetStr();
        NStr::ToLower(idString);
        hasMatch = x_HasMatch(matchVersions, idString, annots);
    }

    if (!hasMatch) {
        annots.clear();
    }
    return annots;

}

void CWholeFileAnnotation::AddAnnot(CRef<CSeq_annot> pAnnot)
{
    auto pAnnotId = GetAnnotId(*pAnnot);
    if (!pAnnotId) {
        return;
    }

    auto idString = pAnnotId->GetSeqIdString();
    NStr::ToLower(idString);
    auto it = m_AnnotMap.find(idString);
    if (it == m_AnnotMap.end()) {
        m_AnnotMap.emplace(idString, list<CRef<CSeq_annot>>{pAnnot});
    }
    else {
        it->second.push_back(pAnnot);
    }
}




END_NCBI_SCOPE
