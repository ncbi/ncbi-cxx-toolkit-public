#ifndef NETCACHE__MIRRORING__HPP
#define NETCACHE__MIRRORING__HPP

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


#include <map>
#include <deque>
#include <string>

#include <corelib/ncbitype.h>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbithr.hpp>

#include "sync_log.hpp"


BEGIN_NCBI_SCOPE

class CNCMirroring
{
public:
    static bool Initialize(void);
    static void Finalize(void);

    static void BlobWriteEvent(const string& key,
                               Uint2 slot,
                               Uint8 orig_rec_no,
                               Uint8 size);
    static void BlobProlongEvent(const string& key,
                                 Uint2 slot,
                                 Uint8 orig_rec_no,
                                 Uint8 orig_time);

    static Uint8 GetQueueSize(void);
    static Uint8 GetQueueSize(Uint8 server_id);


    static CAtomicCounter   sm_TotalCopyRequests;
    static CAtomicCounter   sm_CopyReqsRejected;
};


struct SNCMirrorEvent
{
    ENCSyncEvent evt_type;
    Uint2   slot;
    string  key;
    Uint8   orig_rec_no;
    Uint8   orig_time;


    SNCMirrorEvent(ENCSyncEvent typ,
                   Uint2 slot_,
                   const string& key_,
                   Uint8 rec_no)
        : evt_type(typ),
          slot(slot_),
          key(key_),
          orig_rec_no(rec_no),
          orig_time(0)
    {}
    SNCMirrorEvent(ENCSyncEvent typ,
                   Uint2 slot_,
                   const string& key_,
                   Uint8 rec_no,
                   Uint8 tm)
        : evt_type(typ),
          slot(slot_),
          key(key_),
          orig_rec_no(rec_no),
          orig_time(tm)
    {}
};

typedef list<SNCMirrorEvent*>   TNCMirrorQueue;


class CNCMirroringThread;

struct SDistribution
{
    TNCMirrorQueue big_cmds;
    TNCMirrorQueue small_cmds;
    CFastMutex     lock;
    CNCMirroringThread** handling_threads;
};


enum ENCMirroringType {
    eMirrorSmallPrefered,
    eMirrorSmallExclusive,
    eMirrorLargePrefered
};

class CNCMirroringThread : public CThread
{
public:
    CNCMirroringThread(Uint8            server_id,
                       SDistribution*   distr,
                       ENCMirroringType mirror_type);

    void WakeUp(void)
    {
        notifier.Post();
    }

    void RequestFinish(void)
    {
        finish_flag = true;
        WakeUp();
    }

private:
    virtual void* Main(void);

    Uint8           server_id;
    SDistribution*  distr;
    ENCMirroringType mirror_type;
    bool            finish_flag;
    CSemaphore      notifier;
};


END_NCBI_SCOPE

#endif /* NETCACHE__MIRRORING__HPP */

