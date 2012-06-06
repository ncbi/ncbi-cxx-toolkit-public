#ifndef NETCACHE__MEMORY_MAN__HPP
#define NETCACHE__MEMORY_MAN__HPP
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
 * File Description: 
 */


#include "srv_stat.hpp"


BEGIN_NCBI_SCOPE


struct SSrvThread;
class CSrvPrintProxy;


void InitMemoryMan(void);
void AssignThreadMemMgr(SSrvThread* thr);
void ReleaseThreadMemMgr(SSrvThread* thr);
size_t GetMemSize(void* ptr);



static const Uint2 kMMCntBlocksInPool = 100;
static const Uint2 kMMDrainBatchSize = 35;
static const Uint2 kMMCntBlockSizes = 39;
static const Uint1 kMMCntFreeGrades = 8;
static const int kMMFlushPeriod = 60;

static const Uint4 kMMAllocPageSize = 65536;
static const size_t kMMAllocPageMask = ~size_t(kMMAllocPageSize - 1);
static const Uint2 kMMOSPageSize = 4096;
static const size_t kMMOSPageMask = ~size_t(kMMOSPageSize - 1);


struct SMMBlocksPool
{
    CMiniMutex pool_lock;
    Uint2 size_idx;
    Uint2 cnt_avail;
    Uint2 put_idx;
    Uint2 get_idx;
    void* blocks[kMMCntBlocksInPool];
};


struct SMMPageHeader
{
    size_t block_size;
    void* free_list;
    CMiniMutex page_lock;
    Uint2 cnt_free;
    Uint1 free_grade;
    SMMPageHeader* next_page;
    SMMPageHeader* prev_page;
};


struct SMMFreePageList
{
    CMiniMutex list_lock;
    SMMPageHeader list_head;
};


struct SMMFreePageGrades
{
    SMMFreePageList lists[kMMCntFreeGrades];
};


class CMMFlusher : public CSrvTask
{
public:
    CMMFlusher(void);
    virtual ~CMMFlusher(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


struct SMMStateStat
{
    Uint8 m_UserBlocks[kMMCntBlockSizes];
    Uint8 m_SysBlocks[kMMCntBlockSizes];
    Uint8 m_BigBlocksCnt;
    Uint8 m_BigBlocksSize;
    Uint8 m_TotalSys;
    Uint8 m_TotalData;
};


struct SMMStat
{
    void InitStartState(void);
    void TransferEndState(SMMStat* src_stat);
    void CopyStartState(SMMStat* src_stat);
    void CopyEndState(SMMStat* src_stat);
    void SaveEndState(void);
    void ClearStats(void);
    void AddStats(SMMStat* src_stat);
    void SaveEndStateStat(SMMStat* src_stat);

    void PrintToLogs(CRequestContext* ctx, CSrvPrintProxy& proxy);
    void PrintToSocket(CSrvPrintProxy& proxy);

    void x_PrintUnstructured(CSrvPrintProxy& proxy);


    SMMStateStat m_StartState;
    SMMStateStat m_EndState;
    Uint8 m_UserBlAlloced[kMMCntBlockSizes];
    Uint8 m_UserBlFreed[kMMCntBlockSizes];
    Uint8 m_SysBlAlloced[kMMCntBlockSizes];
    Uint8 m_SysBlFreed[kMMCntBlockSizes];
    Uint8 m_BigAllocedCnt;
    Uint8 m_BigAllocedSize;
    Uint8 m_BigFreedCnt;
    Uint8 m_BigFreedSize;
    CSrvStatTerm<Uint8> m_TotalSysMem;
    CSrvStatTerm<Uint8> m_TotalDataMem;
};


struct SMMMemPoolsSet
{
    Uint4 flush_counter;
    SMMBlocksPool pools[kMMCntBlockSizes];
    SMMStat stat;
};

END_NCBI_SCOPE

#endif /* NETCACHE__MEMORY_MAN__HPP */
