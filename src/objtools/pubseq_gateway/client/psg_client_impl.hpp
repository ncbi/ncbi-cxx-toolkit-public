#ifndef OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_IMPL_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__PSG_CLIENT_IMPL_HPP

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
 * Authors: Rafael Sadyrov
 *
 */

#include <objtools/pubseq_gateway/client/psg_client.hpp>

#include "psg_client_transport.hpp"

#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/netservice_api.hpp>

#include <unordered_map>
#include <mutex>

BEGIN_NCBI_SCOPE

struct SHCT
{
    static HCT::io_coordinator& GetIoc()
    {
        static HCT::io_coordinator rv;
        return rv;
    }

    static shared_ptr<HCT::http2_end_point> GetEndPoint(const string& service_name);

private:
    using TServiceMap = unordered_map<string, CNetService>;

    static CNetService GetService(const string& service_name);

    static pair<mutex, TServiceMap> m_ServiceMap;
};

struct SPSG_BlobReader : IReader
{
    SPSG_BlobReader(SPSG_Reply::SItem::TTS* src);

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    ERW_Result PendingCount(size_t* count);

private:
    void CheckForNewChunks();
    ERW_Result x_Read(void* buf, size_t count, size_t* bytes_read);

    SPSG_Reply::SItem::TTS* m_Src;
    deque<SPSG_Reply::SChunk> m_Data;
    size_t m_Chunk = 0;
    size_t m_Part = 0;
    size_t m_Index = 0;
};

struct SPSG_RStream : private SPSG_BlobReader, private array<char, 64 * 1024>, public CRStream
{
    SPSG_RStream(SPSG_Reply::SItem::TTS* src) :
        SPSG_BlobReader(src),
        CRStream(this, size(), data())
    {}
};

struct CPSG_ReplyItem::SImpl
{
    SPSG_Reply::SItem::TTS* item = nullptr;
};

struct CPSG_Reply::SImpl
{
    shared_ptr<SPSG_Reply::TTS> reply;
    weak_ptr<CPSG_Reply> user_reply;

    shared_ptr<CPSG_ReplyItem> Create(SPSG_Reply::SItem::TTS* item_ts);
};

struct CPSG_Queue::SImpl
{
    SImpl(const string& service);

    bool SendRequest(shared_ptr<const CPSG_Request> request, CDeadline deadline);
    shared_ptr<CPSG_Reply> GetNextReply(CDeadline deadline);
    void Reset();
    bool IsEmpty() const;

private:
    static string GetQuery(const CPSG_Request_Biodata* request_biodata);
    static string GetQuery(const CPSG_Request_Resolve* request_resolve);
    static string GetQuery(const CPSG_Request_Blob* request_blob);

    struct SRequest;
    using TRequests = list<SRequest>;

    shared_ptr<SPSG_ThreadSafe<TRequests>> m_Requests;
    const string m_Service;
};

struct CPSG_Queue::SImpl::SRequest
{
    SRequest(shared_ptr<const CPSG_Request> user_request,
            shared_ptr<HCT::http2_request> http_request,
            shared_ptr<SPSG_Reply::TTS> reply);

    shared_ptr<CPSG_Reply> GetNextReply();
    void Reset();
    bool IsEmpty() const;

private:
    shared_ptr<const CPSG_Request> m_UserRequest;
    shared_ptr<HCT::http2_request> m_HttpRequest;
    shared_ptr<SPSG_Reply::TTS> m_Reply;
};

END_NCBI_SCOPE

#endif
