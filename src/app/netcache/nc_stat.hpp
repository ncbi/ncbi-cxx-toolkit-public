#ifndef NETCACHE__NC_STAT__HPP
#define NETCACHE__NC_STAT__HPP
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
 */


#include "srv_stat.hpp"


BEGIN_NCBI_SCOPE


struct SConstCharCompare
{
    bool operator() (const char* left, const char* right) const;
};


struct SNCStateStat
{
    CSrvTime state_time;
    Uint4 progress_cmds;
    Uint4 db_files;
    Uint8 db_size;
    Uint8 db_garb;
    Uint8 cnt_blobs;
    Uint8 cnt_keys;
    int min_dead_time;
    Uint8 mirror_queue_size;
    Uint8 sync_log_size;
    size_t wb_size;
    size_t wb_releasable;
    size_t wb_releasing;
};


/// Class collecting statistics about NetCache server.
class CNCStat : public CObject
{
public:
    static void Initialize(void);

    static CSrvRef<CNCStat> GetStat(const string& stat_type, bool is_prev);
    static Uint4 GetCntRunningCmds(void);
    static void DumpAllStats(void);
    void PrintToSocket(CSrvSocketTask* sock);
    void PrintState(CSrvSocketTask& sock);

    static void AddSyncServer(Uint8 srv_id);
    static bool AddUnknownServer(Uint8 srv_id);
    static void InitialSyncDone(Uint8 srv_id, bool succeeded);

    static void CmdStarted(const char* cmd);
    static void CmdFinished(const char* cmd, Uint8 len_usec, int status);
    static void ConnClosing(Uint8 cnt_cmds);

    static void ClientDataWrite(size_t data_size);
    static void ClientDataRead(size_t data_size);
    static void ClientBlobWrite(Uint8 blob_size, Uint8 len_usec);
    static void ClientBlobRollback(Uint8 written_size);
    static void ClientBlobRead(Uint8 blob_size, Uint8 len_usec);
    static void PeerDataWrite(size_t data_size);
    static void PeerDataRead(size_t data_size);
    static void PeerSyncFinished(Uint8 srv_id, Uint8 cnt_ops, bool success);
    static void DiskDataWrite(size_t data_size);
    static void DiskDataRead(size_t data_size);
    static void DiskBlobWrite(Uint8 blob_size);
    static void DBFileCleaned(bool success, Uint4 seen_recs,
                              Uint4 moved_recs, Uint4 moved_size);
    static void SaveCurStateStat(const SNCStateStat& state);

public:
    CNCStat(void);

    void InitStartState(void);
    void TransferEndState(CNCStat* src_stat);
    void AddAllStats(CNCStat* src_stat);
    static void CollectThreads(CNCStat* dst_stat, bool need_clear);
    void PrintToLogs(CTempString stat_name);

    typedef map<const char*, TSrvTimeTerm, SConstCharCompare>   TCmdLensMap;
    typedef map<const char*, Uint8, SConstCharCompare>          TCmdCountsMap;
    typedef map<int, TCmdLensMap>                               TStatusCmdLens;

private:
    CNCStat(const CNCStat&);
    CNCStat& operator= (const CNCStat&);

    void x_ClearStats(void);
    void x_AddStats(CNCStat* src_stat);
    void x_CopyStartState(CNCStat* src_stat);
    void x_CopyEndState(CNCStat* src_stat);
    void x_SaveEndState(void);
    void x_PrintUnstructured(CSrvPrintProxy& proxy);


    CMiniMutex m_StatLock;
    string m_StatName;
    SNCStateStat m_StartState;
    SNCStateStat m_EndState;
    Uint8 m_StartedCmds;
    TSrvTimeTerm m_CmdLens;
    TCmdCountsMap m_CmdsByName;
    TStatusCmdLens m_LensByStatus;
    CSrvStatTerm<Uint8> m_ConnCmds;
    Uint8 m_ClDataWrite;
    Uint8 m_ClDataRead;
    Uint8 m_PeerDataWrite;
    Uint8 m_PeerDataRead;
    Uint8 m_DiskDataWrite;
    Uint8 m_DiskDataRead;
    Uint8 m_MaxBlobSize;
    Uint8 m_ClWrBlobs;
    Uint8 m_ClWrBlobSize;
    vector<TSrvTimeTerm> m_ClWrLenBySize;
    Uint8 m_ClRbackBlobs;
    Uint8 m_ClRbackSize;
    Uint8 m_ClRdBlobs;
    Uint8 m_ClRdBlobSize;
    vector<TSrvTimeTerm> m_ClRdLenBySize;
    Uint8 m_DiskWrBlobs;
    Uint8 m_DiskWrBlobSize;
    vector<Uint8> m_DiskWrBySize;
    Uint8 m_PeerSyncs;
    Uint8 m_PeerSynOps;
    Uint8 m_CntCleanedFiles;
    Uint8 m_CntFailedFiles;
    CSrvStatTerm<Uint4> m_CheckedRecs;
    CSrvStatTerm<Uint4> m_MovedRecs;
    CSrvStatTerm<Uint4> m_MovedSize;
    CSrvStatTerm<Uint4> m_CntFiles;
    CSrvStatTerm<Uint8> m_DBSize;
    CSrvStatTerm<Uint8> m_GarbageSize;
    CSrvStatTerm<Uint8> m_CntBlobs;
    CSrvStatTerm<Uint8> m_CntKeys;
    CSrvStatTerm<Uint8> m_MirrorQSize;
    CSrvStatTerm<Uint8> m_SyncLogSize;
    CSrvStatTerm<size_t> m_WBMemSize;
    CSrvStatTerm<size_t> m_WBReleasable;
    CSrvStatTerm<size_t> m_WBReleasing;
    auto_ptr<CSrvStat> m_SrvStat;
};


class CStatRotator : public CSrvTask
{
public:
    CStatRotator(void);
    virtual ~CStatRotator(void);

    void CalcNextRun(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


END_NCBI_SCOPE

#endif /* NETCACHE__NC_STAT__HPP */
