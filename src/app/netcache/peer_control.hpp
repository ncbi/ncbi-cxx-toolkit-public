#ifndef NETCACHE__PEER_CONTROL__HPP
#define NETCACHE__PEER_CONTROL__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */


#include "sync_log.hpp"
#include "nc_db_info.hpp"


BEGIN_NCBI_SCOPE


class CNCActiveHandler;
class CNCActiveClientHub;
class CNCBlobAccessor;
class CNCActiveSyncControl;

struct SActiveList_tag;
typedef intr::list_base_hook<intr::tag<SActiveList_tag> >   TActiveListHook;
typedef intr::list<CNCActiveHandler,
                   intr::base_hook<TActiveListHook>,
                   intr::constant_time_size<false> >        TNCPeerConnsList;
typedef list<CNCActiveClientHub*>   TNCClientHubsList;
typedef list<CNCActiveSyncControl*> TNCActiveSyncList;
typedef TNCActiveSyncList::iterator TNCActiveSyncListIt;


struct SNCMirrorEvent
{
    ENCSyncEvent evt_type;
    Uint2   slot;
    string  key;
    Uint8   orig_rec_no;


    SNCMirrorEvent(ENCSyncEvent typ,
                   Uint2 slot_,
                   const string& key_,
                   Uint8 rec_no)
        : evt_type(typ),
          slot(slot_),
          key(key_),
          orig_rec_no(rec_no)
    {}
};

struct SNCMirrorProlong : public SNCMirrorEvent
{
    Uint8 orig_time;
    SNCBlobSummary blob_sum;


    SNCMirrorProlong(ENCSyncEvent typ,
                     Uint2 slot_,
                     const string& key_,
                     Uint8 rec_no,
                     Uint8 tm,
                     const CNCBlobAccessor* accessor);
};

typedef list<SNCMirrorEvent*>   TNCMirrorQueue;



class CNCPeerControl : public CSrvTask
{
public:
    static bool Initialize(void);
    static void Finalize(void);

    static void SetServersForInitSync(Uint4 cnt_servers);
    static bool HasServersForInitSync(void);

    static CNCPeerControl* Peer(Uint8 srv_id);

    static void MirrorWrite(const string& key,
                            Uint2 slot,
                            Uint8 orig_rec_no,
                            Uint8 size);
    static void MirrorProlong(const string& key,
                              Uint2 slot,
                              Uint8 orig_rec_no,
                              Uint8 orig_time,
                              const CNCBlobAccessor* accessor);
    static Uint8 GetMirrorQueueSize(void);
    static Uint8 GetMirrorQueueSize(Uint8 srv_id);

    void SetSlotsForInitSync(Uint2 cnt_slots);
    void AddInitiallySyncedSlot(void);
    void RegisterSyncStop(bool is_passive,
                          Uint8& next_sync_time,
                          Uint8 next_sync_delay);
    Uint8 GetNextSyncTime(void);

    Uint8 GetSrvId(void);
    CNCActiveHandler* GetBGConn(void);
    bool StartActiveSync(void);
    bool AddSyncControl(CNCActiveSyncControl* sync_ctrl);
    bool FinishSync(CNCActiveSyncControl* sync_ctrl);

    void RegisterConnError(void);
    void RegisterConnSuccess(void);
    void AssignClientConn(CNCActiveClientHub* hub);
    CNCActiveHandler* GetPooledConn(void);
    bool CreateNewSocket(CNCActiveHandler* conn);
    void PutConnToPool(CNCActiveHandler* conn);
    void ReleaseConn(CNCActiveHandler* conn);

    bool GetReadyForShutdown(void);


    static CAtomicCounter   sm_TotalCopyRequests;
    static CAtomicCounter   sm_CopyReqsRejected;


private:
    CNCPeerControl(Uint8 srv_id);
    CNCPeerControl(const CNCPeerControl&);
    CNCPeerControl& operator= (const CNCPeerControl&);

    virtual void ExecuteSlice(TSrvThreadNum thr_num);

    void x_SrvInitiallySynced(void);
    void x_SlotsInitiallySynced(Uint2 cnt_slots);
    CNCActiveHandler* x_GetPooledConnImpl(void);
    bool x_ReserveBGConn(void);
    void x_UnreserveBGConn(void);
    void x_IncBGConns(void);
    void x_DecBGConns(void);
    void x_DecBGConns(CNCActiveHandler* conn);
    void x_DecActiveConns(void);
    CNCActiveHandler* x_CreateNewConn(bool for_bg);
    bool x_AssignClientConn(CNCActiveClientHub* hub, CNCActiveHandler* conn);
    CNCActiveHandler* x_GetBGConnImpl(void);
    bool x_DoReleaseConn(CNCActiveHandler* conn);
    void x_DeleteMirrorEvent(SNCMirrorEvent* event);
    void x_ProcessMirrorEvent(CNCActiveHandler* conn, SNCMirrorEvent* event);
    void x_AddMirrorEvent(SNCMirrorEvent* event, Uint8 size);
    void x_UpdateHasTasks(void);


    Uint8 m_SrvId;
    CMiniMutex m_ObjLock;
    TNCPeerConnsList m_PooledConns;
    TNCPeerConnsList m_BusyConns;
    Uint8 m_FirstNWErrTime;
    Uint8 m_ThrottleStart;
    Uint8 m_NextSyncTime;
    Uint2 m_ActiveConns;
    Uint2 m_BGConns;
    Uint2 m_SlotsToInitSync;
    Uint2 m_CntActiveSyncs;
    Uint1 m_CntNWErrors;
    bool  m_InThrottle;
    bool  m_HasBGTasks;
    bool  m_InitiallySynced;
    TNCClientHubsList m_Clients;
    TNCMirrorQueue m_SmallMirror;
    TNCMirrorQueue m_BigMirror;
    TNCActiveSyncList m_SyncList;
    TNCActiveSyncListIt m_NextTaskSync;
};


class CNCPeerShutdown : public CSrvShutdownCallback
{
public:
    CNCPeerShutdown(void);
    virtual ~CNCPeerShutdown(void);

    virtual bool ReadyForShutdown(void);
};



//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline void
CNCPeerControl::SetSlotsForInitSync(Uint2 cnt_slots)
{
    m_SlotsToInitSync = cnt_slots;
}

inline Uint8
CNCPeerControl::GetSrvId(void)
{
    return m_SrvId;
}

inline Uint8
CNCPeerControl::GetNextSyncTime(void)
{
    return m_NextSyncTime;
}

END_NCBI_SCOPE

#endif /* NETCACHE__PEER_CONTROL__HPP */
