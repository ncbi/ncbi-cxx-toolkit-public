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

// ICC does not like this array to be a direct base of SPSG_RStream
struct SPSG_BlobReader : IReader, protected array<char, 64 * 1024>
{
    using TStats = pair<bool, weak_ptr<SPSG_Stats>>;
    SPSG_BlobReader(SPSG_Reply::SItem::TTS& src, TStats stats = TStats());

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    ERW_Result PendingCount(size_t* count);

private:
    void CheckForNewChunks(vector<SPSG_Chunk>& chunks);
    ERW_Result x_Read(SPSG_Reply::SItem& src, void* buf, size_t count, size_t* bytes_read);

    SPSG_Reply::SItem::TTS& m_Src;
    TStats m_Stats;
    vector<SPSG_Chunk> m_Data;
    size_t m_Chunk = 0;
    size_t m_Index = 0;
};

struct SPSG_RStream : private SPSG_BlobReader, public CRStream
{
    template <class... TArgs>
    SPSG_RStream(TArgs&&... args) :
        SPSG_BlobReader(std::forward<TArgs>(args)...),
        CRStream(this, size(), data())
    {}
};

struct CPSG_ReplyItem::SImpl
{
    SPSG_Reply::SItem::TTS& item;
    SImpl(SPSG_Reply::SItem::TTS& i) : item(i) {}
};

struct CPSG_Reply::SImpl
{
    shared_ptr<SPSG_Reply> reply;
    weak_ptr<CPSG_Reply> user_reply;

    shared_ptr<CPSG_ReplyItem> Create(SPSG_Reply::SItem::TTS& item_ts);

private:
    template <class TReplyItem>
    CPSG_ReplyItem* CreateImpl(TReplyItem* item, const vector<SPSG_Chunk>& chunks);
    CPSG_ReplyItem* CreateImpl(SPSG_Reply::SItem::TTS& item_ts, const SPSG_Args& args, shared_ptr<SPSG_Stats>& stats);
    CPSG_ReplyItem* CreateImpl(CPSG_SkippedBlob::EReason reason, const SPSG_Args& args, shared_ptr<SPSG_Stats>& stats);
    CPSG_ReplyItem* CreateImpl(SPSG_Reply::SItem::TTS& item_ts, SPSG_Reply::SItem& item, CPSG_ReplyItem::EType type, CPSG_SkippedBlob::EReason reason);
};

struct SPSG_UserArgsBuilder
{
    SPSG_UserArgsBuilder() { x_UpdateCache(); }
    void SetQueueArgs(SPSG_UserArgs queue_args) { m_QueueArgs = std::move(queue_args); x_UpdateCache(); }
    void Build(ostream& os, const SPSG_UserArgs& request_args);
    void BuildRaw(ostringstream& os, const SPSG_UserArgs& request_args);

private:
    void x_MergeOthers(SPSG_UserArgs& combined_args);
    void x_UpdateCache();

    struct MergeValues; // A function-like class
    static bool Merge(SPSG_UserArgs& higher_priority, const SPSG_UserArgs& lower_priority);

    static SPSG_UserArgs s_ReadIniArgs();
    static const SPSG_UserArgs& s_GetIniArgs();
    
    SPSG_UserArgs m_QueueArgs;
    string m_CachedArgs;
};

struct CPSG_Queue::SImpl
{
    shared_ptr<TPSG_Queue> queue;

    SImpl(const string& service);

    bool SendRequest(shared_ptr<CPSG_Request> request, CDeadline deadline);
    shared_ptr<CPSG_Reply> SendRequestAndGetReply(shared_ptr<CPSG_Request> request, CDeadline deadline);
    bool WaitForEvents(CDeadline deadline);
    auto& GetQueues() { return m_Service.ioc.queues; }

    bool RejectsRequests() const { return m_Service.ioc.RejectsRequests(); }
    void SetRequestFlags(CPSG_Request::TFlags request_flags) { m_RequestFlags = request_flags; }
    void SetUserArgs(SPSG_UserArgs user_args) { m_UserArgsBuilder.GetLock()->SetQueueArgs(std::move(user_args)); }

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

    string x_GetAbsPathRef(shared_ptr<const CPSG_Request> user_request, const CPSG_Request::TFlags& flags, bool raw);

    CService m_Service;
    CPSG_Request::TFlags m_RequestFlags = CPSG_Request::eDefaultFlags;
    SThreadSafe<SPSG_UserArgsBuilder> m_UserArgsBuilder;
};

inline ostream& operator<<(ostream& os, const SPSG_UserArgs& request_args)
{
    for (const auto& p : request_args) {
        for (const auto& s : p.second) {
            os << '&' << p.first << '=' << s;
        }
    }

    return os;
}

END_NCBI_SCOPE

#endif
#endif
