#ifndef MY_NCBI_CACHE__HPP
#define MY_NCBI_CACHE__HPP

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
#include "myncbi_callback.hpp"
#include <objtools/pubseq_gateway/impl/myncbi/myncbi_request.hpp>

USING_NCBI_SCOPE;


class CPubseqGatewayApp;


struct SMyNCBIOKCacheItem
{
    public:
        enum EPSGS_MyNCBIResolutionStatus {
            ePSGS_Ready,            // user info is available
            ePSGS_InProgress        // somebody has requested user info and there is still no answer
        };

    public:
        SMyNCBIOKCacheItem() :
            m_LastTouch(psg_clock_t::now()),
            m_Status(ePSGS_InProgress)
        {}

        ~SMyNCBIOKCacheItem()
        {}

    public:
        struct SMyNCBIWaitListItem
        {
            IPSGS_Processor *   m_Processor;
            TMyNCBIDataCB       m_DataCB;
            TMyNCBIErrorCB      m_ErrorCB;
        };

        void x_AddToWaitList(IPSGS_Processor *  processor,
                             TMyNCBIDataCB   data_cb,
                             TMyNCBIErrorCB   error_cb)
        {
            m_WaitList.push_back(SMyNCBIWaitListItem{processor, data_cb, error_cb});
        }
        void x_OnSuccess(const string &  cookie,
                         const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info);
        void x_OnError(const string &  cookie, CRequestStatus::ECode  status,
                             int  code, EDiagSev  severity, const string &  message);
        void x_OnNotFound(const string &  cookie);
        void x_RemoveWaiter(IPSGS_Processor *  processor);

    public:
        psg_time_point_t                        m_LastTouch;
        CPSG_MyNCBIRequest_WhoAmI::SUserInfo    m_UserInfo;
        EPSGS_MyNCBIResolutionStatus            m_Status;
        list<SMyNCBIWaitListItem>               m_WaitList;
};



class CMyNCBIOKCache
{
    public:
        struct SUserInfoItem
        {
            SMyNCBIOKCacheItem::EPSGS_MyNCBIResolutionStatus    m_Status;
            CPSG_MyNCBIRequest_WhoAmI::SUserInfo                m_UserInfo;
        };

    public:
        CMyNCBIOKCache(CPubseqGatewayApp *  app, size_t  high_mark, size_t  low_mark) :
            m_App(app), m_HighMark(high_mark), m_LowMark(low_mark)
        {}

        ~CMyNCBIOKCache()
        {}

    public:
        optional<SUserInfoItem> GetUserInfo(IPSGS_Processor *  processor,
                                            const string &  cookie,
                                            TMyNCBIDataCB  data_cb,
                                            TMyNCBIErrorCB  error_cb);
        void AddUserInfo(const string &  cookie,
                         const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info);
        void OnError(const string &  cookie, CRequestStatus::ECode  status,
                     int  code, EDiagSev  severity, const string &  message);
        void OnNotFound(const string &  cookie);

        void ClearWaitingProcessor(const string &  cookie,
                                   IPSGS_Processor *  processor);
        void ClearInitiatedRequest(const string &  cookie);

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
        map<string, SMyNCBIOKCacheItem>     m_Cache;
        list<string>                        m_LRU;
        mutex                               m_Lock;
};




class CMyNCBINotFoundCache
{
    public:
        CMyNCBINotFoundCache(CPubseqGatewayApp *  app,
                             size_t  high_mark, size_t  low_mark,
                             size_t  expiration_sec) :
            m_App(app), m_HighMark(high_mark), m_LowMark(low_mark),
            m_ExpirationMs(expiration_sec * 1000)
        {}

        ~CMyNCBINotFoundCache()
        {}

    public:
        bool IsNotFound(const string &  cookie);
        void AddNotFound(const string &  cookie);

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
        size_t                              m_ExpirationMs;

        map<string, psg_time_point_t>       m_Cache;
        list<string>                        m_LRU;
        mutex                               m_Lock;
};



struct SMyNCBIErrorCacheItem
{
    public:
        SMyNCBIErrorCacheItem(CRequestStatus::ECode  status, int  code,
                              EDiagSev  severity, const string &  message) :
            m_LastTouch(psg_clock_t::now()),
            m_Status(status), m_Code(code), m_Severity(severity), m_Message(message)
        {}

        SMyNCBIErrorCacheItem()
        {}

        ~SMyNCBIErrorCacheItem()
        {}

    public:
        psg_time_point_t        m_LastTouch;

        // Error information
        CRequestStatus::ECode   m_Status;
        int                     m_Code;
        EDiagSev                m_Severity;
        string                  m_Message;
};


class CMyNCBIErrorCache
{
    public:
        CMyNCBIErrorCache(CPubseqGatewayApp *  app,
                          size_t  high_mark, size_t  low_mark,
                          size_t  back_off_ms) :
            m_App(app), m_HighMark(high_mark), m_LowMark(low_mark),
            m_BackOffMs(back_off_ms)
        {}

        ~CMyNCBIErrorCache()
        {}

    public:
        optional<SMyNCBIErrorCacheItem> GetError(const string &  cookie);
        void AddError(const string &  cookie, CRequestStatus::ECode  status,
                      int  code, EDiagSev  severity, const string &  message);

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
        size_t                              m_BackOffMs;

        map<string, SMyNCBIErrorCacheItem>  m_Cache;
        list<string>                        m_LRU;
        mutex                               m_Lock;
};


#endif

