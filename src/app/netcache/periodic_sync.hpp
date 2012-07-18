#ifndef NETCACHE__PERIODIC_SYNC__HPP
#define NETCACHE__PERIODIC_SYNC__HPP

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
 * Authors: Pavel Ivanov
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */

#include "sync_log.hpp"
#include "nc_db_info.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCPeerControl;
class CNCActiveHandler;
struct SNCCacheData;


enum ESyncInitiateResult {
    eNetworkError,
    eServerBusy,            //< Clean is in progress
    eCrossSynced,
    eProceedWithEvents,     //< The sync can proceed basing on events log
    eProceedWithBlobs       //< The sync can proceed basing on full list of blobs
};


// Front end for periodic synchronization
// The interface is for sync initiated by another peer only!
// The initiative from the current peer is coming from the [hidden]
// CNCPeerSyncThread.
class CNCPeriodicSync
{
public:
    static bool Initialize(void);
    static void Finalize(void);

    // Initiates synchronization which came from a peer netcache.
    // It can be rejected if another sync is in progress or there are no
    // records available anymore
    static ESyncInitiateResult Initiate(Uint8  server_id,
                                        Uint2  slot,
                                        Uint8* local_start_rec_no,
                                        Uint8* remote_start_rec_no,
                                        TReducedSyncEvents* events,
                                        Uint8* sync_id);

    // Cancels the current sync
    static void Cancel(Uint8 server_id, Uint2 slot, Uint8 sync_id);

    static ESyncInitiateResult CanStartSyncCommand(Uint8  server_id,
                                                   Uint2  slot,
                                                   bool   can_abort,
                                                   Uint8& sync_id);
    static void MarkCurSyncByBlobs(Uint8 server_id, Uint2 slot, Uint8 sync_id);
    static void SyncCommandFinished(Uint8 server_id, Uint2 slot, Uint8 sync_id);

    // Completes the current sync transaction
    static void Commit(Uint8 server_id,
                       Uint2 slot,
                       Uint8 sync_id,
                       Uint8 local_synced_rec_no,
                       Uint8 remote_synced_rec_no);
};



enum ESyncStatus
{
    eIdle,              // Vacant for something
    eSyncInProgress,    // Periodic sync is in progress (could be events based or blob lists based)
    eCleanInProgress    // Events log cleaning is in progress
};

enum ESyncResult {
    eSynOK,
    eSynCrossSynced,
    eSynServerBusy,
    eSynNetworkError,
    eSynAborted
};

enum ESynTaskType {
    eSynNoTask,
    eSynNeedFinalize,
    eSynEventSend,
    eSynEventGet,
    eSynBlobUpdateOur,
    eSynBlobUpdatePeer,
    eSynBlobSend,
    eSynBlobGet
};

enum ESynActionType {
    eSynActionNone,
    eSynActionRead,
    eSynActionWrite,
    eSynActionProlong,
    eSynActionRemove
};


struct SSyncSrvData
{
    CMiniMutex lock;
    Uint8   srv_id;
    Uint8   next_sync_time;
    Uint8   first_nw_err_time;
    Uint2   slots_to_init;
    Uint2   cnt_active_syncs;
    bool    initially_synced;

    SSyncSrvData(Uint8 srv_id);
};

struct SSyncSlotSrv
{
    CMiniMutex lock;
    CNCPeerControl* peer;
    bool    sync_started;
    bool    is_passive;
    bool    is_by_blobs;
    bool    was_blobs_sync;
    bool    made_initial_sync;
    Uint4   started_cmds;
    Uint8   next_sync_time;
    Uint8   last_active_time;
    Uint8   cur_sync_id;
    Uint8   cnt_sync_ops;

    SSyncSlotSrv(CNCPeerControl* peer);
};

typedef map<Uint2, SSyncSlotSrv*>  TSlotSrvsList;


struct SSyncSlotData
{
    CMiniMutex lock;
    TSlotSrvsList srvs;
    Uint2   slot;
    Uint1   cnt_sync_started;
    bool    cleaning;
    bool    clean_required;

    SSyncSlotData(Uint2 slot);
};

typedef map<Uint2, SSyncSlotData*>  TSyncSlotsMap;


typedef TSyncEvents::const_iterator     TSyncEventsIt;
typedef TNCBlobSumList::const_iterator  TBlobsListIt;

struct SSyncTaskInfo
{
    ESynTaskType task_type;
    TSyncEventsIt get_evt;
    TSyncEventsIt send_evt;
    TBlobsListIt local_blob;
    TBlobsListIt remote_blob;
};


/*
    manages sychronization (blob data and metadata) between NC servers
    
    begin: x_StartScanSlots

    -> x_StartScanSlots: pick first slot, goto  x_CheckSlotOurSync

    -> x_CheckSlotOurSync
        when all slots processed, goto x_FinishScanSlots
        for a given slot, check if it is time to sync; if yes, goto x_DoPeriodicSync
        goto x_CheckSlotTheirSync

    -> x_CheckSlotTheirSync
        check if there is a sync on this slot started by somebody else and was not active for too long
            if yes, cancel it
        pick next slot, goto x_CheckSlotOurSync

    -> x_FinishScanSlots
            calc when to run next time
            goto x_StartScanSlots

    ->  x_DoPeriodicSync:
            once again, check that the sync does not start from both sides at the same time
                if so, goto x_CheckSlotTheirSyn
            get CNCActiveHandler which sends start sync (execute SyncStart command)
                ok ? x_WaitSyncStarted : x_FinishSync

    -> x_WaitSyncStarted
            wait for sync started (check m_StartedCmds)
                NCActiveHandler will report command result using  CmdFinished() method
            depending on the reply, goto x_PrepareSyncByBlobs, or goto x_PrepareSyncByEvents

    -> x_PrepareSyncByEvents
            another server has sent us list of events,
            now give this list to  CNCSyncLog, which will gve us the difference  (m_Events2Get,m_Events2Send)
            if list not empty, goto x_ExecuteSyncCommands
            
            if CNCSyncLog cannot sync event lists (eg, some our info is lost), request blob list
                goto x_WaitForBlobList

    -> x_WaitForBlobList
            once blob list received, goto x_PrepareSyncByBlobs
    
    -> x_PrepareSyncByBlobs
            re-fill list of local blobs (in given slot)
            goto x_ExecuteSyncCommands
    
    -> x_ExecuteSyncCommands
            if no more commands, goto x_ExecuteFinalize
            if network error, goto x_FinishSync
            goto x_WaitForExecutingTasks

    -> x_ExecuteFinalize
            on success, finish sync, goto  x_WaitForExecutingTasks
            on error,  goto x_FinishSync

    -> x_WaitForExecutingTasks
            woken up by CmdFinished
            if all commands are executed ok, and need 'commit', goto x_ExecuteFinalize
            after commit is done, goto x_FinishSync

    -> x_FinishSync
            log report
            CommitSync
            goto x_CheckSlotTheirSync

*/

class CNCActiveSyncControl : public CSrvStatesTask<CNCActiveSyncControl>
{
public:
    CNCActiveSyncControl(void);
    virtual ~CNCActiveSyncControl(void);

    Uint2 GetSyncSlot(void);
    void StartResponse(Uint8 local_rec_no, Uint8 remote_rec_no, bool by_blobs);
    void AddStartEvent(SNCSyncEvent* evt);
    void AddStartBlob(const string& key, SNCBlobSummary* blob_sum);
    bool GetNextTask(SSyncTaskInfo& task_info);
    void ExecuteSyncTask(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void CmdFinished(ESyncResult res, ESynActionType action, CNCActiveHandler* conn);

private:
    State x_StartScanSlots(void);
    State x_CheckSlotOurSync(void);
    State x_CheckSlotTheirSync(void);
    State x_FinishScanSlots(void);
    State x_DoPeriodicSync(void);
    State x_WaitSyncStarted(void);
    State x_PrepareSyncByEvents(void);
    State x_WaitForBlobList(void);
    State x_PrepareSyncByBlobs(void);
    State x_ExecuteSyncCommands(void);
    State x_ExecuteFinalize(void);
    State x_WaitForExecutingTasks(void);
    State x_FinishSync(void);
    void x_CleanRemoteObjects(void);
    void x_CalcNextTask(void);
    void x_DoEventSend(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void x_DoEventGet(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void x_DoBlobUpdateOur(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void x_DoBlobUpdatePeer(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void x_DoBlobSend(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void x_DoBlobGet(const SSyncTaskInfo& task_info, CNCActiveHandler* conn);
    void x_DoFinalize(CNCActiveHandler* conn);


    SSyncSlotData*  m_SlotData;
    SSyncSlotSrv*   m_SlotSrv;
    Uint8       m_SrvId;
    Uint2       m_Slot;
    ESyncResult m_Result;
    CMiniMutex  m_Lock;
    Uint4       m_StartedCmds;

    Uint8 m_LocalStartRecNo;
    Uint8 m_RemoteStartRecNo;
    Uint8 m_LocalSyncedRecNo;
    Uint8 m_RemoteSyncedRecNo;
    ESynTaskType m_NextTask;
    TReducedSyncEvents m_RemoteEvents;
    TSyncEvents    m_Events2Get;
    TSyncEvents    m_Events2Send;
    TSyncEventsIt  m_CurGetEvent;
    TSyncEventsIt  m_CurSendEvent;
    TNCBlobSumList m_LocalBlobs;
    TNCBlobSumList m_RemoteBlobs;
    TBlobsListIt   m_CurLocalBlob;
    TBlobsListIt   m_CurRemoteBlob;
    Uint8   m_ReadOK;
    Uint8   m_ReadERR;
    Uint8   m_WriteOK;
    Uint8   m_WriteERR;
    Uint8   m_ProlongOK;
    Uint8   m_ProlongERR;
    Uint8   m_DelOK;
    Uint8   m_DelERR;
    bool m_ForceInitSync;
    bool m_NeedRehash;
    bool m_DidSync;
    bool m_FinishSyncCalled;
    Uint8 m_MinNextTime;
    Uint8 m_LoopStart;
    TSyncSlotsMap::const_iterator m_NextSlotIt;
    Uint8 m_StartTime;
};


class CNCLogCleaner : public CSrvTask
{
public:
    CNCLogCleaner(void);
    virtual ~CNCLogCleaner(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_idx);


    TSyncSlotsMap::const_iterator m_NextSlotIt;
    map<Uint2, Uint8> m_LastForceTime;
};




//////////////////////////////////////////////////////////////////////////
//  Inline functions
//////////////////////////////////////////////////////////////////////////

inline Uint2
CNCActiveSyncControl::GetSyncSlot(void)
{
    return m_Slot;
}

inline void
CNCActiveSyncControl::StartResponse(Uint8 local_rec_no,
                                    Uint8 remote_rec_no,
                                    bool by_blobs)
{
    m_LocalStartRecNo = local_rec_no;
    m_RemoteStartRecNo = remote_rec_no;
    m_SlotSrv->is_by_blobs = by_blobs;
}

inline void
CNCActiveSyncControl::AddStartEvent(SNCSyncEvent* evt)
{
    if (evt->event_type == eSyncProlong)
        m_RemoteEvents[evt->key].prolong_event = evt;
    else
        m_RemoteEvents[evt->key].wr_or_rm_event = evt;
}

inline void
CNCActiveSyncControl::AddStartBlob(const string& key, SNCBlobSummary* blob_sum)
{
    m_RemoteBlobs[key] = blob_sum;
}

END_NCBI_SCOPE


#endif /* NETCACHE__PERIODIC_SYNC__HPP */

