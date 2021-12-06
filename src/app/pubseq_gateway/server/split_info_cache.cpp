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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "split_info_cache.hpp"



pair<bool, CRef<CID2S_Split_Info>>
CSplitInfoCache::GetBlob(const SCass_BlobId &  info_blob_id)
{
    auto    last_touch = std::chrono::steady_clock::now();

    while (m_Lock.exchange(true)) {}    // acquire top level lock

    auto    it = m_Cache.find(info_blob_id);
    if (it == m_Cache.cend()) {
        m_Lock = false;                 // release top level lock
        return make_pair(false, CRef<CID2S_Split_Info>());
    }

    it->second.m_LastTouch = last_touch;
    auto    ret = make_pair(true, it->second.m_SplitInfoBlob);

    m_Lock = false;                     // release top level lock
    return ret;
}


void
CSplitInfoCache::AddBlob(const SCass_BlobId &  info_blob_id,
                         CRef<CID2S_Split_Info>  blob)
{
    while (m_Lock.exchange(true)) {}    // acquire top level lock

    m_Cache[info_blob_id] = SSplitInfoCacheItem(blob);

    m_Lock = false;                     // release top level lock
}

