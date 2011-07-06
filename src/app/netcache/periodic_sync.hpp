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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */


#include <map>
#include <deque>
#include <string>

// For Uint2, Uint8, NCBI_CONST_UINT8
#include <corelib/ncbitype.h>

// For CFastMutex
#include <corelib/ncbimtx.hpp>

// For BEGIN_NCBI_SCOPE
#include <corelib/ncbistl.hpp>

#include <corelib/ncbithr.hpp>

#include "sync_log.hpp"
#include "nc_db_info.hpp"
#include "nc_utils.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE



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
    static void PreInitialize(void);
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

    static Uint4 BeginTimeEvent(Uint8 server_id);
    static void EndTimeEvent(Uint8 server_id, Uint8 adj_time);
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
    eSynActionRead,
    eSynActionWrite,
    eSynActionProlong,
    eSynActionRemove
};


struct SSyncSrvData
{
    CFastMutex lock;
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
    CFastMutex lock;
    SSyncSrvData* srv_data;
    bool    sync_started;
    bool    is_passive;
    bool    is_by_blobs;
    bool    was_blobs_sync;
    bool    made_initial_sync;
    Uint4   started_cmds;
    Uint8   next_sync_time;
    Uint8   last_active_time;
    Uint8   cur_sync_id;

    SSyncSlotSrv(SSyncSrvData* srv_data);
};

typedef map<Uint2, SSyncSlotSrv*>  TSlotSrvsList;


struct SSyncSlotData
{
    CFastMutex lock;
    TSlotSrvsList srvs;
    Uint2   slot;
    Uint1   cnt_sync_started;
    bool    cleaning;
    bool    clean_required;

    SSyncSlotData(Uint2 slot);
};

typedef map<Uint2, SSyncSlotData*>  TSyncSlotsMap;


class CActiveSyncControl
{
public:
    CActiveSyncControl(void);

    bool DoPeriodicSync(SSyncSlotData* slot_data, SSyncSlotSrv*  slot_srv);
    ESynTaskType GetSynTaskType(void);
    void ExecuteSynTask(ESynTaskType task_type);
    void FinalizeSync(void);
    void WakeUp(void);

private:
    void x_PrepareSyncByEvents(Uint8 local_start_rec_no,
                               Uint8 remote_start_rec_no,
                               const TReducedSyncEvents& remote_events,
                               Uint8* local_synced_rec_no,
                               Uint8* remote_synced_rec_no);
    void x_PrepareSyncByBlobs(Uint8* local_synced_rec_no);
    void x_DoEventSend(ENCPeerFailure& task_res, ESynActionType& action);
    void x_DoEventGet(ENCPeerFailure& task_res, ESynActionType& action);
    void x_DoBlobUpdateOur(ENCPeerFailure& task_res, ESynActionType& action);
    void x_DoBlobUpdatePeer(ENCPeerFailure& task_res, ESynActionType& action);
    void x_DoBlobSend(ENCPeerFailure& task_res, ESynActionType& action);
    void x_DoBlobGet(ENCPeerFailure& task_res, ESynActionType& action);


    SSyncSlotData*  m_SlotData;
    SSyncSlotSrv*   m_SlotSrv;
    CRef<CRequestContext> m_DiagCtx;
    Uint8       m_SrvId;
    Uint2       m_Slot;
    ESyncResult m_Result;
    CSemaphore  m_WaitSem;
    CFastMutex  m_Lock;
    bool        m_HasTasks;
    Uint4       m_StartedCmds;

    TSyncEvents     m_Events2Get;
    TSyncEvents     m_Events2Send;
    TSyncEvents::const_iterator m_CurGetEvent;
    TSyncEvents::const_iterator m_CurSendEvent;
    TNCBlobSumList  m_LocalBlobs;
    TNCBlobSumList  m_RemoteBlobs;
    TNCBlobSumList::const_iterator  m_CurLocalBlob;
    TNCBlobSumList::const_iterator  m_CurRemoteBlob;
    Uint8   m_ReadOK;
    Uint8   m_ReadERR;
    Uint8   m_WriteOK;
    Uint8   m_WriteERR;
    Uint8   m_ProlongOK;
    Uint8   m_ProlongERR;
    Uint8   m_DelOK;
    Uint8   m_DelERR;
};


class CNCTimeThrottler
{
public:
    CNCTimeThrottler(void);

    Uint4 BeginTimeEvent(Uint8 server_id);
    void EndTimeEvent(Uint8 server_id, Uint8 adj_time);

private:
    Uint8   m_PeriodStart;
    Uint8   m_TotalTime;
    Uint8   m_CurStart;
    typedef map<Uint8, Uint8>   TTimeMap;
    TTimeMap m_SrvTime;

    enum EMode {
        eTotal,
        eByServer
    };
    EMode   m_Mode;
};


class CNCThrottler_Getter : public CNCTlsObject<CNCThrottler_Getter,
                                                CNCTimeThrottler>
{
public:
    static CNCTimeThrottler* CreateTlsObject(void);
    static void DeleteTlsObject(void* obj);
};


END_NCBI_SCOPE


#endif /* NETCACHE__PERIODIC_SYNC__HPP */

