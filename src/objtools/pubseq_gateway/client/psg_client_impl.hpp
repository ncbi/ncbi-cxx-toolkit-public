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
 * Author: Rafael Sadyrov
 *
 */

#include <objtools/pubseq_gateway/client/impl/misc.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

#ifdef HAVE_PSG_CLIENT

#include "psg_client_transport.hpp"

#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>

#include <unordered_map>
#include <mutex>

BEGIN_NCBI_SCOPE

struct SPSG_BlobReader : IReader
{
    SPSG_BlobReader(SPSG_Reply::SItem::TTS* src);

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    ERW_Result PendingCount(size_t* count);

private:
    void CheckForNewChunks();
    ERW_Result x_Read(void* buf, size_t count, size_t* bytes_read);

    SPSG_Reply::SItem::TTS* m_Src;
    vector<SPSG_Chunk> m_Data;
    size_t m_Chunk = 0;
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
    shared_ptr<SPSG_Reply> reply;
    weak_ptr<CPSG_Reply> user_reply;

    shared_ptr<CPSG_ReplyItem> Create(SPSG_Reply::SItem::TTS* item_ts);

private:
    template <class TReplyItem>
    TReplyItem* CreateImpl(TReplyItem* item, const vector<SPSG_Chunk>& chunks);
};

struct CPSG_Queue::SImpl
{
    shared_ptr<TPSG_Queue> queue;

    SImpl(const string& service);

    bool SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline);
    shared_ptr<CPSG_Reply> SendRequestAndGetReply(shared_ptr<CPSG_Request> request, CDeadline deadline);

    bool WaitForEvents(CDeadline deadline)
    {
        _ASSERT(queue);
        return queue->CV().WaitUntil(queue->Stopped(), move(deadline));
    }

    bool RejectsRequests() const { return m_Service.ioc.RejectsRequests(); }

    static TApiLock GetApiLock() { return CService::GetMap(); }

private:
    class CService
    {
        // Have to use unique_ptr as some old compilers do not use move ctor of SPSG_IoCoordinator
        using TMap = unordered_map<string, unique_ptr<SPSG_IoCoordinator>>;

        SPSG_IoCoordinator& GetIoC(const string& service);

        shared_ptr<TMap> m_Map;
        static pair<mutex, weak_ptr<TMap>> sm_Instance;

    public:
        SPSG_IoCoordinator& ioc;

        CService(const string& service) : m_Map(GetMap()), ioc(GetIoC(service)) {}

        static shared_ptr<TMap> GetMap();
    };

    string x_GetAbsPathRef(shared_ptr<const CPSG_Request> user_request);

    CService m_Service;
};

END_NCBI_SCOPE

#endif
#endif
