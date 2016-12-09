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
#include <objects/seq/Bioseq.hpp>

#include "base_report.hpp"
#include "seqid_hook.hpp"
#include "seq_entry_hook.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

CSkipSeqEntryHook::CSkipSeqEntryHook(CBaseReport& report) :
    m_report(report),
    m_level(0)
{}

void CSkipSeqEntryHook::SkipObject(CObjectIStream &in, const CObjectTypeInfo &info)
{
    ++m_level;

    if (m_level == 1) {

        CObjectTypeInfo type = CType<CBioseq>();
        type.FindMember("id").SetLocalSkipHook(in, new CSkipSeqIdHook(m_report));

        DefaultSkip(in, info);

        m_report.ClearCurrentSeqId();
        // do not call ResetLocalSkipHook here, because CSkipSeqIdHook class will do that by itself
    }
    else
        DefaultSkip(in, info);

    --m_level;
}

} // namespace pub_report