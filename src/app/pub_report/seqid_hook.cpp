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

#include <objects/seqloc/Seq_id.hpp>

#include "base_report.hpp"
#include "seqid_hook.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

static CConstRef<CSeq_id> GetBestId(const CBioseq::TId& ids)
{
    CConstRef<CSeq_id> best_seq_id;

    int best_score = INT_MAX;
    ITERATE(CBioseq::TId, it, ids) {

        if (CSeq_id::e_Genbank == (*it)->Which()) {
            return *it;
        }

        int score = (*it)->BaseBestRankScore();
        if (best_score > score) {
            best_seq_id = *it;
            best_score = score;
        }
    }

    return best_seq_id;
}

CSkipSeqIdHook::CSkipSeqIdHook(CBaseReport& report) :
    m_report(report)
{};

void CSkipSeqIdHook::SkipClassMember(CObjectIStream &in, const CObjectTypeInfoMI &member)
{
    CBioseq::TId ids;
    CObjectInfo info(&ids, (*member).GetTypeInfo());
    in.ReadObject(info);

    CConstRef<CSeq_id> best_seq_id = GetBestId(ids);

    if (best_seq_id.NotEmpty()) {
        string label;
        best_seq_id->GetLabel(&label);
        m_report.SetCurrentSeqId(label);
    }

    member.ResetLocalSkipHook(in);
}

} // namespace pub_report