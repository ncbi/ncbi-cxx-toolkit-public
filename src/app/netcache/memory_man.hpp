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

//try to use standard memory manager
#if defined(NETCACHE_MEMORY_MAN_STD) || defined(NETCACHE_MEMORY_MAN_TCM)
#define __NC_MEMMAN_USE_STD_MALLOC 1
#else
#define __NC_MEMMAN_USE_STD_MALLOC 0
#endif

/*
    there were occasional crashes in s_DoUnmap (munmap fails)
    this new approach seems to fix the problem
*/
#define __NC_MEMMAN_NEWALLOC 1
/*
    if I use real_ptr, I have to change (reduce) kNCMaxBlobChunkSize
    while everything seems to work fine, when mixing old and new NC, still it is scary.
    so, let us try to keep kNCMaxBlobChunkSize as it always was
*/
#define __NC_MEMMAN_USEREALPTR 0

BEGIN_NCBI_SCOPE


struct SSrvThread;
class CSrvPrintProxy;


void InitMemoryMan(void);
void FinalizeMemoryMan(void);
void AssignThreadMemMgr(SSrvThread* thr);
void ReleaseThreadMemMgr(SSrvThread* thr);
size_t GetMemSize(void* ptr);
Int8   GetMPageCount(void);

static const Uint2 kMMCntBlockSizes = 39;

struct SMMStateStat
{
    SMMStateStat(void) {
        memset(this, 0, sizeof(SMMStateStat));
    }
    Int8 m_UserBlocks[kMMCntBlockSizes];
    Int8 m_SysBlocks[kMMCntBlockSizes];
    Int8 m_BigBlocksCnt;
    Int8 m_BigBlocksSize;
    Uint8 m_TotalSys;
    Uint8 m_TotalData;
};


struct SMMStat
{
    SMMStat(void) {
//        ClearStats();
    }

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
    void PrintState(CSrvSocketTask& task);

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

END_NCBI_SCOPE

#endif /* NETCACHE__MEMORY_MAN__HPP */
