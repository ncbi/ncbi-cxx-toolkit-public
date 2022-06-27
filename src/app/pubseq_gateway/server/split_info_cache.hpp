#ifndef SPLIT_INFO_CACHE__HPP
#define SPLIT_INFO_CACHE__HPP

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


#include <mutex>
#include <map>
#include <list>
using namespace std;

#include "pubseq_gateway_types.hpp"
#include "cass_blob_id.hpp"
#include "split_info_utils.hpp"
#include <objects/seqsplit/seqsplit__.hpp>

USING_NCBI_SCOPE;

using namespace ncbi::objects;

struct SSplitInfoCacheItem
{
    public:
        SSplitInfoCacheItem(CRef<CID2S_Split_Info> blob) :
            m_LastTouch(psg_clock_t::now()),
            m_SplitInfoBlob(blob)
        {}

        SSplitInfoCacheItem()
        {}

        ~SSplitInfoCacheItem()
        {}

    public:
        // For the future - if the cache needs to be purged
        psg_time_point_t            m_LastTouch;
        CRef<CID2S_Split_Info>      m_SplitInfoBlob;
};



class CSplitInfoCache
{
    public:
        CSplitInfoCache(size_t  high_mark, size_t  low_mark) :
            m_HighMark(high_mark), m_LowMark(low_mark)
        {}

        ~CSplitInfoCache()
        {}

    public:
        pair<bool, CRef<CID2S_Split_Info>>
                                GetBlob(const SCass_BlobId &  info_blob_id);
        void                    AddBlob(const SCass_BlobId &  info_blob_id,
                                        CRef<CID2S_Split_Info>);

        size_t Size(void)
        {
            size_t              size = 0;
            lock_guard<mutex>   guard(m_Lock);

            size = m_Cache.size();
            return size;
        }

        // Cleans up the cache if needed
        void Maintain(void);

    private:
        size_t                                      m_HighMark;
        size_t                                      m_LowMark;
        map<SCass_BlobId, SSplitInfoCacheItem>      m_Cache;
        list<SCass_BlobId>                          m_LRU;
        mutex                                       m_Lock;
};


#endif
