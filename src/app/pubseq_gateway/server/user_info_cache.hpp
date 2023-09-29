#ifndef USER_INFO_CACHE__HPP
#define USER_INFO_CACHE__HPP

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
#include <string>
#include <optional>
using namespace std;

#include "pubseq_gateway_types.hpp"
#include <objtools/pubseq_gateway/impl/myncbi/myncbi_request.hpp>

USING_NCBI_SCOPE;


class CPubseqGatewayApp;


struct SUserInfoCacheItem
{
    public:
        SUserInfoCacheItem(const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info) :
            m_LastTouch(psg_clock_t::now()),
            m_UserInfo(user_info)
        {}

        SUserInfoCacheItem()
        {}

        ~SUserInfoCacheItem()
        {}

    public:
        // For the future - if the cache needs to be purged
        psg_time_point_t                        m_LastTouch;
        CPSG_MyNCBIRequest_WhoAmI::SUserInfo    m_UserInfo;
};



class CUserInfoCache
{
    public:
        CUserInfoCache(CPubseqGatewayApp *  app, size_t  high_mark, size_t  low_mark) :
            m_App(app), m_HighMark(high_mark), m_LowMark(low_mark)
        {}

        ~CUserInfoCache()
        {}

    public:
        optional<CPSG_MyNCBIRequest_WhoAmI::SUserInfo>
                                GetUserInfo(const string &  cookie);
        void                    AddUserInfo(const string &  cookie,
                                            const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info);

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
        CPubseqGatewayApp *                 m_App;

        size_t                              m_HighMark;
        size_t                              m_LowMark;
        map<string, SUserInfoCacheItem>     m_Cache;
        list<string>                        m_LRU;
        mutex                               m_Lock;
};


#endif

