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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_socket.hpp>

#include "mirroring.hpp"
#include "distribution_conf.hpp"
#include "netcached.hpp"


BEGIN_NCBI_SCOPE


typedef map<Uint8, SDistribution*>  TDistributiorsMap;
static TDistributiorsMap            s_Distributors;
static CAtomicCounter               s_TotalQueueSize;
static FILE*                        s_LogFile = NULL;
CAtomicCounter        CNCMirroring::sm_TotalCopyRequests;
CAtomicCounter        CNCMirroring::sm_CopyReqsRejected;



CNCMirroringThread::CNCMirroringThread(Uint8            server_id_,
                                       SDistribution*   distr_,
                                       ENCMirroringType mirror_type_)
    : server_id(server_id_),
      distr(distr_),
      mirror_type(mirror_type_),
      finish_flag(false),
      notifier(0, 1000000000)
{}

void*
CNCMirroringThread::Main(void)
{
    while (!finish_flag)
    {
        SNCMirrorEvent* event = NULL;
        distr->lock.Lock();
        switch (mirror_type) {
        case eMirrorLargePrefered:
            if (!distr->big_cmds.empty()) {
                event = distr->big_cmds.back();
                distr->big_cmds.pop_back();
                break;
            }
            // fall through
        case eMirrorSmallExclusive:
            if (!distr->small_cmds.empty()) {
                event = distr->small_cmds.back();
                distr->small_cmds.pop_back();
            }
            break;
        case eMirrorSmallPrefered:
            if (!distr->small_cmds.empty()) {
                event = distr->small_cmds.back();
                distr->small_cmds.pop_back();
                break;
            }
            if (!distr->big_cmds.empty()) {
                event = distr->big_cmds.back();
                distr->big_cmds.pop_back();
            }
        }
        distr->lock.Unlock();

        if (event) {
            int queue_size = s_TotalQueueSize.Add(-1);
            if (s_LogFile) {
                Uint8 cur_time = CNetCacheServer::GetPreciseTime();
                fprintf(s_LogFile, NCBI_BIGCOUNT_FORMAT_SPEC ",%d,%d\n",
                        TNCBI_BigCount(cur_time), queue_size, CThread::GetSelf());
            }
            ENCPeerFailure send_res = ePeerActionOK;
            switch (event->evt_type) {
            case eSyncWrite:
                send_res = CNetCacheServer::SendBlobToPeer(
                                            server_id, event->slot, event->key,
                                            event->orig_rec_no, false);
                break;
            case eSyncProlong:
                send_res = CNetCacheServer::ProlongBlobOnPeer(
                                            server_id, event->slot, event->key,
                                            event->orig_rec_no, event->orig_time);
                break;
            }
            if (send_res != ePeerActionOK)
                CNCMirroring::sm_CopyReqsRejected.Add(1);

            delete event;
            continue;
        }
        notifier.Wait();
    }

    return 0;
}


bool
CNCMirroring::Initialize(void)
{
    const TNCPeerList& peers = CNCDistributionConf::GetPeers();

    Uint1 cnt_threads = CNCDistributionConf::GetCntMirroringThreads();
    Uint1 small_prefered = CNCDistributionConf::GetMirrorSmallPrefered();
    Uint1 small_exclusive = CNCDistributionConf::GetMirrorSmallExclusive();
    ITERATE(TNCPeerList, it_peer, peers) {
        Uint8 server_id = it_peer->first;
        SDistribution* distr = new SDistribution;
        s_Distributors[server_id] = distr;
        distr->handling_threads = (CNCMirroringThread**)malloc(cnt_threads * sizeof(*distr->handling_threads));

        for (Uint1 i = 0; i < small_prefered; ++i) {
            distr->handling_threads[i] = new CNCMirroringThread(server_id, distr, eMirrorSmallPrefered);
        }
        for (Uint1 i = 0; i < small_exclusive; ++i) {
            Uint1 ind = small_prefered + i;
            distr->handling_threads[ind] = new CNCMirroringThread(server_id, distr, eMirrorSmallExclusive);
        }
        for (Uint1 i = small_prefered + small_exclusive; i < cnt_threads; ++i) {
            distr->handling_threads[i] = new CNCMirroringThread(server_id, distr, eMirrorLargePrefered);
        }
        try {
            for (Uint1 i = 0; i < cnt_threads; ++i) {
                distr->handling_threads[i]->Run();
            }
        }
        catch (CThreadException& ex) {
            ERR_POST(Critical << ex);
            return false;
        }
    }

    s_TotalQueueSize.Set(0);
    s_LogFile = fopen(CNCDistributionConf::GetMirroringSizeFile().c_str(), "a");
    sm_TotalCopyRequests.Set(0);
    sm_CopyReqsRejected.Set(0);
    return true;
}

void
CNCMirroring::Finalize(void)
{
    Uint1 cnt_threads = CNCDistributionConf::GetCntMirroringThreads();

    ITERATE(TDistributiorsMap, it, s_Distributors) {
        for (Uint1 i = 0; i < cnt_threads; ++i) {
            it->second->handling_threads[i]->RequestFinish();
        }
    }
    try {
        ITERATE(TDistributiorsMap, it, s_Distributors) {
            for (Uint1 i = 0; i < cnt_threads; ++i) {
                it->second->handling_threads[i]->Join();
            }
        }
    }
    catch (CThreadException& ex) {
        ERR_POST(Critical << ex);
    }

    if (s_LogFile)
        fclose(s_LogFile);
}

static void
s_QueueEvent(SDistribution* distr, SNCMirrorEvent* event, Uint8 size)
{
    CFastMutexGuard guard(distr->lock);
    TNCMirrorQueue* q;
    if (size <= CNCDistributionConf::GetSmallBlobBoundary())
        q = &distr->small_cmds;
    else
        q = &distr->big_cmds;

    CNCMirroring::sm_TotalCopyRequests.Add(1);
    if (q->size() < 10000) {
        q->push_back(event);
        int queue_size = s_TotalQueueSize.Add(1);
        if (s_LogFile) {
            Uint8 cur_time = CNetCacheServer::GetPreciseTime();
            fprintf(s_LogFile, NCBI_BIGCOUNT_FORMAT_SPEC ",%d,%d\n",
                    TNCBI_BigCount(cur_time), queue_size, CThread::GetSelf());
        }

        if (q->size() == 1) {
            Uint1 cnt_threads = CNCDistributionConf::GetCntMirroringThreads();
            for (Uint1 i = 0; i < cnt_threads; ++i) {
                distr->handling_threads[i]->WakeUp();
            }
        }
    }
    else {
        CNCMirroring::sm_CopyReqsRejected.Add(1);
        delete event;
    }
}

void
CNCMirroring::BlobWriteEvent(const string& key,
                             Uint2 slot,
                             Uint8 orig_rec_no,
                             Uint8 size)
{
    const TServersList& servers = CNCDistributionConf::GetRawServersForSlot(slot);
    ITERATE(TServersList, it_srv, servers) {
        Uint8 srv_id = *it_srv;
        SDistribution* distr = s_Distributors[srv_id];
        SNCMirrorEvent* event = new SNCMirrorEvent(eSyncWrite, slot, key, orig_rec_no);
        s_QueueEvent(distr, event, size);
    }
}

void
CNCMirroring::BlobProlongEvent(const string& key,
                               Uint2 slot,
                               Uint8 orig_rec_no,
                               Uint8 orig_time)
{
    const TServersList& servers = CNCDistributionConf::GetRawServersForSlot(slot);
    ITERATE(TServersList, it_srv, servers) {
        Uint8 srv_id = *it_srv;
        SDistribution* distr = s_Distributors[srv_id];
        SNCMirrorEvent* event = new SNCMirrorEvent(eSyncProlong, slot, key,
                                                   orig_rec_no, orig_time);
        s_QueueEvent(distr, event, 0);
    }
}

Uint8
CNCMirroring::GetQueueSize(void)
{
    return s_TotalQueueSize.Get();
}

Uint8
CNCMirroring::GetQueueSize(Uint8 server_id)
{
    SDistribution* distr = s_Distributors[server_id];
    CFastMutexGuard guard(distr->lock);
    return distr->small_cmds.size() + distr->big_cmds.size();
}

END_NCBI_SCOPE

