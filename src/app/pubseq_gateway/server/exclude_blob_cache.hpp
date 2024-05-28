#ifndef EXCLUDE_BLOB_CACHE__HPP
#define EXCLUDE_BLOB_CACHE__HPP

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


#include <string>
#include <set>
#include <list>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <optional>
#include <corelib/ncbistr.hpp>
using namespace std;

#include "pubseq_gateway_types.hpp"


enum EPSGS_CacheAddResult {
    ePSGS_AlreadyInCache,
    ePSGS_Added
};


struct SExcludeBlobId
{
    int                 m_Sat;
    int                 m_SatKey;
    optional<string>    m_BlobId;
    bool                m_Completed;
    psg_time_point_t    m_CompletedTime;

    SExcludeBlobId(int  sat, int  sat_key):
        m_Sat(sat), m_SatKey(sat_key), m_Completed(false)
    {}

    SExcludeBlobId(const string& blob_id, int chunk_id = 0) :
        m_Sat(0), m_SatKey(chunk_id), m_BlobId(blob_id), m_Completed(false)
    {}

    bool operator < (const SExcludeBlobId &  other) const
    {
        if (m_BlobId) {
            if (!other.m_BlobId) return true;
            auto cmp = m_BlobId->compare(*other.m_BlobId);
            return cmp ? cmp < 0 : m_SatKey < other.m_SatKey;
        }
        if (other.m_BlobId) return false;
        if (m_Sat == other.m_Sat)
            return m_SatKey < other.m_SatKey;
        return m_Sat < other.m_Sat;
    }

    bool operator == (const SExcludeBlobId &  other) const
    {
        if (m_BlobId) {
            if (!other.m_BlobId) return false;
            return m_SatKey == other.m_SatKey && *m_BlobId == *other.m_BlobId;
        }
        if (other.m_BlobId) return false;
        return m_Sat == other.m_Sat && m_SatKey == other.m_SatKey;
    }
};



class CUserExcludeBlobs
{
    public:
        CUserExcludeBlobs() :
            m_LastTouch(psg_clock_t::now())
        {}

        ~CUserExcludeBlobs()
        {}

    public:
        // The 'completed' and 'completed_time' values are filled
        // only if the blob is in the cache
        bool IsInCache(const SExcludeBlobId& blob_id,
                       bool &  completed,
                       psg_time_point_t &  completed_time);
        EPSGS_CacheAddResult AddBlobId(const SExcludeBlobId& blob_id,
                                       bool &  completed,
                                       psg_time_point_t &  completed_time);

        // Return true if the required blob id was found
        bool SetCompleted(const SExcludeBlobId& blob_id, bool  new_val);
        bool Remove(const SExcludeBlobId& blob_id);

        void Purge(size_t  purged_size);
        void Clear(void);

    public:
        // The lock is exposed so that the upper level can grab it before
        // the upper lock is released. An alternative would be to store the
        // lock in the upper level but it seems better to store it here because
        // it goes into the cathegory of the user associated data
        mutex                       m_Lock;
        psg_time_point_t            m_LastTouch;

    private:
        set<SExcludeBlobId>         m_ExcludeBlobs;
        list<SExcludeBlobId>        m_LRU;
};


class CUserExcludeBlobsPool
{
    public:
        CUserExcludeBlobsPool() :
            m_Head(nullptr)
        {}

        ~CUserExcludeBlobsPool()
        {
            SNode *     current = m_Head.load();
            while (current != nullptr) {
                delete current->m_Data;
                auto next_item = current->m_Next;
                delete current;
                current = next_item;
            }
        }

        // Non copyable
        CUserExcludeBlobsPool(CUserExcludeBlobsPool const &) = delete;
        CUserExcludeBlobsPool(CUserExcludeBlobsPool &&) = delete;
        const CUserExcludeBlobsPool & operator=(const CUserExcludeBlobsPool &) = delete;

    public:
        CUserExcludeBlobs *  Get(void)
        {
            SNode *     item = m_Head.load();
            if (item == nullptr)
                return new CUserExcludeBlobs();

            m_Lock.lock();

            item = m_Head.load();
            auto object = item->m_Data;
            m_Head = item->m_Next;

            m_Lock.unlock();

            delete item;
            return object;
        }

        void Return(CUserExcludeBlobs *  user_exclude_blobs)
        {
            SNode *     returned_node = new SNode(user_exclude_blobs);
            lock_guard<mutex>   guard(m_Lock);

            returned_node->m_Next = m_Head.load();
            m_Head = returned_node;
        }

    private:
        struct SNode {
            SNode *                 m_Next;
            CUserExcludeBlobs *     m_Data;

            SNode(CUserExcludeBlobs *  exclude_blobs) :
                m_Next(nullptr), m_Data(exclude_blobs)
            {}
        };

        atomic<SNode *>     m_Head;
        mutex               m_Lock;
};



class CExcludeBlobCache
{
    public:
        CExcludeBlobCache(size_t  inactivity_timeout,
                          size_t  max_cache_size, size_t  purged_size) :
            m_InactivityTimeout(inactivity_timeout),
            m_MaxCacheSize(max_cache_size), m_PurgedSize(purged_size)
        {
            m_ToPurge.reserve(128);     // arbitrary reservation
            m_ToDiscard.reserve(128);   // arbitrary reservation
        }

        ~CExcludeBlobCache()
        {
            for (auto &  item : m_UserBlobs)
                delete item.second;
        }

    public:
        // The 'completed' and 'completed_time' values are filled only
        // if the blob is in the cache
        EPSGS_CacheAddResult AddBlobId(const string &  user,
                                       const SExcludeBlobId& blob_id,
                                       bool &  completed,
                                       psg_time_point_t &  completed_time);
        bool IsInCache(const string &  user,
                       const SExcludeBlobId& blob_id,
                       bool &  completed,
                       psg_time_point_t &  completed_time);

        // Return true if the required blob id was found
        bool SetCompleted(const string &  user,
                          const SExcludeBlobId& blob_id, bool  new_val);
        bool Remove(const string &  user,
                    const SExcludeBlobId& blob_id);

        void Purge(void);

        size_t Size(void) {
            size_t size = 0;
            lock_guard<mutex>   guard(m_Lock);

            size = m_UserBlobs.size();
            return size;
        }

    private:
        map<string, CUserExcludeBlobs *>    m_UserBlobs;
        mutex                               m_Lock;
        CUserExcludeBlobsPool               m_Pool;

        chrono::seconds                     m_InactivityTimeout;
        size_t                              m_MaxCacheSize;
        size_t                              m_PurgedSize;

        vector<CUserExcludeBlobs *>         m_ToPurge;
        vector<CUserExcludeBlobs *>         m_ToDiscard;
};


#endif
