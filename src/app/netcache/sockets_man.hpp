#ifndef NETCACHE__SOCKETS_MAN__HPP
#define NETCACHE__SOCKETS_MAN__HPP
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


#include "task_server.hpp"


BEGIN_NCBI_SCOPE


struct SSrvThread;
struct SSocketsData;


void ConfigureSockets(CNcbiRegistry* reg, CTempString section);
bool InitSocketsMan(void);
bool StartSocketsMan(void);
void DoSocketWait(void);
void FinalizeSocketsMan(void);
void AssignThreadSocks(SSrvThread* thr);
void ReleaseThreadSocks(SSrvThread* thr);
void MoveAllSockets(SSocketsData* dst_socks, SSocketsData* src_socks);
void PromoteSockAmount(SSocketsData* socks);
void CheckConnectsTimeout(SSocketsData* socks);
void CleanSocketList(SSocketsData* socks);
void SetAllSocksRunnable(SSocketsData* socks);
void RequestStopListening(void);
Uint4 SocksGetTotal(void);


static const Uint1 kEpollEventsArraySize = 100;
static const Uint2 kSockReadBufSize = 1000;
static const Uint2 kSockMinWriteSize = 1000;
static const Uint2 kSockWriteBufSize = 2000;
static const Uint1 kMaxCntListeningSocks = 16;


struct SSocketsData
{
    TSockList sock_list;
    Int2 sock_cnt;
};


struct SListenSockInfo : public SSrvSocketInfo
{
    Uint1 index;
    Uint2 port;
    int fd;
    CSrvSocketFactory* factory;
};


class CSrvListener : public CSrvTask
{
public:
    CSrvListener(void);
    virtual ~CSrvListener(void);

    virtual void ExecuteSlice(TSrvThreadNum thread_idx);

public:
    Uint4 m_SeenEvents[kMaxCntListeningSocks];
    Uint4 m_SeenErrors[kMaxCntListeningSocks];
};


END_NCBI_SCOPE

#endif /* NETCACHE__SOCKETS_MAN__HPP */
