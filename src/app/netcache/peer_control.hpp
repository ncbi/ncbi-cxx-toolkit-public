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
#include "nc_stat.hpp"
#include <set>


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
    CNCBlobKeyLight  key;
    Uint8   orig_rec_no;


    SNCMirrorEvent(ENCSyncEvent typ,
                   Uint2 slot_,
                   const CNCBlobKeyLight& key_,
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
                     const CNCBlobKeyLight& key_,
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

    static Uint4 FindIPbyAlias(Uint4 alias);
    static Uint4 FindIPbyName(const string& alias);
    static bool  HasPeerInThrottle(void);

    static void SetServersForInitSync(Uint4 cnt_servers);
    static void ResetServersForInitSync(void);
    static void ReconfServersForInitSync(Uint4 cnt_servers);
    static bool HasServersForInitSync(void);
    static void PrintState(CSrvSocketTask& task);

    static CNCPeerControl* Peer(Uint8 srv_id);
    static string GetPeerNameOrEmpty(Uint8 srv_id);

    static void MirrorUpdate(const CNCBlobKey& key,
                              Uint2 slot,
                              Uint8 update_time);
    static void MirrorRemove(const CNCBlobKey& key,
                              Uint2 slot,
                              Uint8 update_time);
    static void MirrorWrite(const CNCBlobKey& key,
                            Uint2 slot,
                            Uint8 orig_rec_no,
                            Uint8 size);
    static void MirrorProlong(const CNCBlobKey& key,
                              Uint2 slot,
                              Uint8 orig_rec_no,
                              Uint8 orig_time,
                              const CNCBlobAccessor* accessor);
    static Uint8 GetMirrorQueueSize(void);
    static Uint8 GetMirrorQueueSize(Uint8 srv_id);
    static void ReadCurState(SNCStateStat& state);

    void PeerHandshake(void);
    void SetSlotsForInitSync(Uint2 cnt_slots);
    void ResetSlotsForInitSync();
    void ReconfSlotsForInitSync(Uint2 cnt_slots);
    void AddInitiallySyncedSlot(void);
    void RegisterSyncStop(bool is_passive,
                          Uint8& next_sync_time,
                          Uint8 next_sync_delay);
    Uint8 GetNextSyncTime(void);

    Uint8 GetSrvId(void) const;
    CNCActiveHandler* GetBGConn(void);
    bool StartActiveSync(void);
    bool AddSyncControl(CNCActiveSyncControl* sync_ctrl);
    void RemoveSyncControl(CNCActiveSyncControl* sync_ctrl);
    bool FinishSync(CNCActiveSyncControl* sync_ctrl);

    void RegisterConnError(void);
    void RegisterConnSuccess(void);
    void ConnOk(void);
    void AssignClientConn(CNCActiveClientHub* hub);
    CNCActiveHandler* GetPooledConn(void);
    bool CreateNewSocket(CNCActiveHandler* conn);
    void PutConnToPool(CNCActiveHandler* conn);
    void ReleaseConn(CNCActiveHandler* conn);

    bool GetReadyForShutdown(void);


    static CAtomicCounter   sm_TotalCopyRequests;
    static CAtomicCounter   sm_CopyReqsRejected;

    void AbortInitialSync(void);
    void SetHostProtocol(Uint8 ver);

    bool AcceptsSyncUpdate(void) const;
    bool AcceptsSyncRemove(void) const;
    bool AcceptsBlobKey(const CNCBlobKeyLight& key) const;
    bool AcceptsBList(void) const;

private:
    CNCPeerControl(Uint8 srv_id);
    CNCPeerControl(const CNCPeerControl&);
    CNCPeerControl& operator= (const CNCPeerControl&);

    virtual void ExecuteSlice(TSrvThreadNum thr_num);

    void x_SrvInitiallySynced(bool succeeded);
    void x_SlotsInitiallySynced(Uint2 cnt_slots, bool aborted=false);
    CNCActiveHandler* x_GetPooledConnImpl(void);
    bool x_ReserveBGConn(void);
    bool x_ReserveBGConnNow(void);
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
    void x_ProcessUpdateEvent(SNCMirrorEvent* event);
    void x_ProcessMirrorEvent(CNCActiveHandler* conn, SNCMirrorEvent* event);
    void x_AddMirrorEvent(SNCMirrorEvent* event, Uint8 size);
    void x_UpdateHasTasks(void);


    Uint8 m_SrvId;
    Uint4 m_HostIP;
    string m_HostIPname;
    string m_Hostname;
    Uint4 m_HostAlias;
    Uint8 m_HostProtocol;
    CMiniMutex m_ObjLock;
    TNCPeerConnsList m_PooledConns;
    TNCPeerConnsList m_BusyConns;
    Uint8 m_FirstNWErrTime;
    Uint8 m_ThrottleStart;
    Uint8 m_NextSyncTime;
    int m_ActiveConns;
    int m_BGConns;
    Uint2 m_SlotsToInitSync;
    Uint2 m_OrigSlotsToInitSync;
    Uint2 m_CntActiveSyncs;
    Uint2 m_CntNWErrors;
    Uint2 m_CntNWThrottles;
    bool  m_InThrottle, m_MaybeThrottle;
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
    m_OrigSlotsToInitSync = m_SlotsToInitSync = cnt_slots;
    m_InitiallySynced = m_OrigSlotsToInitSync == 0;
}

inline void
CNCPeerControl::ResetSlotsForInitSync()
{
    SetSlotsForInitSync(m_OrigSlotsToInitSync);
}

inline void
CNCPeerControl::ReconfSlotsForInitSync(Uint2 cnt_slots)
{
    m_OrigSlotsToInitSync = cnt_slots;
}

inline Uint8
CNCPeerControl::GetSrvId(void) const
{
    return m_SrvId;
}

inline Uint8
CNCPeerControl::GetNextSyncTime(void)
{
    return m_NextSyncTime;
}

inline bool 
CNCPeerControl::AcceptsSyncUpdate(void) const
{
    return m_HostProtocol >= 60700;
}
inline bool
CNCPeerControl::AcceptsSyncRemove(void) const
{
    return m_HostProtocol >= 60806;
}
inline bool
CNCPeerControl::AcceptsBlobKey(const CNCBlobKeyLight& key) const
{
    return m_HostProtocol >= 60700 || key.KeyVersion() < 3;
}

inline bool 
CNCPeerControl::AcceptsBList(void) const
{
    return m_HostProtocol >= 60900;
}

inline void
CNCPeerControl::ConnOk(void)
{
    m_MaybeThrottle = false;
}


END_NCBI_SCOPE

#endif /* NETCACHE__PEER_CONTROL__HPP */
