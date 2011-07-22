#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file query_data.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/query_data.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

//
// IQueryFactory
//

CRef<ILocalQueryData>
IQueryFactory::MakeLocalQueryData(const CBlastOptions* options)
{
    if (m_LocalQueryData.Empty()) {
        m_LocalQueryData.Reset(x_MakeLocalQueryData(options));
    }
    return m_LocalQueryData;
}

CRef<IRemoteQueryData>
IQueryFactory::MakeRemoteQueryData()
{
    if (m_RemoteQueryData.Empty()) {
        m_RemoteQueryData.Reset(x_MakeRemoteQueryData());
    }
    return m_RemoteQueryData;
}

//
// ILocalQueryData
//

void
ILocalQueryData::x_ValidateIndex(size_t index)
{
    if (index > GetNumQueries()) {
        throw std::out_of_range("Index " + NStr::SizetToString(index) +
                                " out of range (" +
                                NStr::SizetToString(GetNumQueries()) +
                                " max)");
    }
}

bool
ILocalQueryData::IsValidQuery(size_t index)
{
    x_ValidateIndex(index);

    const BlastQueryInfo* query_info = GetQueryInfo();
    _ASSERT(query_info);

    bool all_contexts_valid = true;
    for (int i = query_info->first_context;
         i <= query_info->last_context; 
         i++) {
        if (query_info->contexts[i].query_index == (int)index) {
            if ( !query_info->contexts[i].is_valid ) {
                all_contexts_valid = false;
                break;
            }
        }
    }
    return all_contexts_valid;
}

size_t
ILocalQueryData::GetSumOfSequenceLengths()
{
    if (m_SumOfSequenceLengths == 0) {
        for (size_t i = 0; i < GetNumQueries(); i++) {
            try { m_SumOfSequenceLengths += GetSeqLength(i); }
            catch (const CBlastException&) {
                ; // ignore errors if the length could not be retrieved
            }
        }
    }
    return m_SumOfSequenceLengths;
}

bool
ILocalQueryData::IsAtLeastOneQueryValid()
{
    bool found_valid_query = false;

    for (size_t i = 0; i < GetNumQueries(); i++) {
        if (IsValidQuery(i)) {
            found_valid_query = true;
            break;
        }
    }
    return found_valid_query;
}

void
ILocalQueryData::GetQueryMessages(size_t index, TQueryMessages& qmsgs)
{
    x_ValidateIndex(index);
    qmsgs = m_Messages[index];
}

void
ILocalQueryData::GetMessages(TSearchMessages& messages) const
{
    messages = m_Messages;
}

void
ILocalQueryData::FlushSequenceData()
{
    m_SeqBlk.Reset();
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
