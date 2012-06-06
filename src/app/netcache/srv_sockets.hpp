#ifndef NETCACHE__SRV_SOCKETS__HPP
#define NETCACHE__SRV_SOCKETS__HPP
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


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


struct SSrvSocketInfo
{
    bool is_listening;
};


struct SSrvSockList_tag;
typedef intr::list_member_hook<intr::tag<SSrvSockList_tag> >    TSrvSockListHook;


class CSrvSocketTask : public virtual CSrvTask, public SSrvSocketInfo
{
public:
    CSrvSocketTask(void);
    virtual ~CSrvSocketTask(void);

public:
    bool IsReadDataAvailable(void);
    bool IsWriteDataPending(void);
    bool HasError(void);
    bool CanHaveMoreRead(void);
    bool NeedToClose(void);
    bool NeedEarlyClose(void);

    bool ReadToBuf(void);
    size_t Read(void* buf, size_t size);
    bool ReadLine(CTempString* line);
    bool ReadData(void* buf, Uint2 size);
    template <typename NumType>
    bool ReadNumber(NumType* num);

    CSrvSocketTask& WriteText(CTempString message);
    CSrvSocketTask& WriteText(const string& message);
    CSrvSocketTask& WriteText(const char* message);
    template <typename NumType>
    CSrvSocketTask& WriteNumber(NumType num);
    void WriteData(const void* buf, size_t size);
    size_t Write(const void* buf, size_t size);
    void Flush(void);
    void RequestFlush(void);
    bool FlushIsDone(void);

    void StartProxyTo(CSrvSocketTask* dst_task, Uint8 proxy_size);
    bool IsProxyInProgress(void);
    bool ProxyHadError(void);

    bool Connect(Uint4 host, Uint2 port);
    bool StartProcessing(TSrvThreadNum thread_num = 0);
    void CloseSocket(void);
    void AbortSocket(void);

    void GetPeerAddress(string& host, Uint2& port);
    Uint2 GetLocalPort(void);

    virtual void Terminate(void);

private:
    CSrvSocketTask(const CSrvSocketTask&);
    CSrvSocketTask& operator= (const CSrvSocketTask&);

    virtual void InternalRunSlice(TSrvThreadNum thr_idx);

    void x_CloseSocket(bool do_abort);
    void x_PrintError(void);

public:
    CSrvSocketTask* m_ProxySrc;
    CSrvSocketTask* m_ProxyDst;
    Uint8 m_ProxySize;
    TSrvSockListHook m_SockListHook;
    char* m_RdBuf;
    char* m_WrBuf;
    Uint4 m_ConnStartJfy;
    int m_LastActive;
    int m_Fd;
    Uint2 m_RdSize;
    Uint2 m_RdPos;
    Uint2 m_WrMemSize;
    Uint2 m_WrSize;
    Uint2 m_WrPos;
    bool m_CRMet;
    bool m_ProxyHadError;
    bool m_SockHasRead;
    bool m_SockCanWrite;
    bool m_SockCanReadMore;
    bool m_NeedToClose;
    bool m_NeedToFlush;
    bool m_FlushIsDone;
    Uint1 m_SeenReadEvts;
    Uint1 m_SeenWriteEvts;
    Uint8 m_ReadBytes;
    Uint8 m_WrittenBytes;
    Uint1 m_RegReadEvts;
    Uint1 m_RegWriteEvts;
    bool m_RegReadHup;
    bool m_RegError;
    bool m_ErrorPrinted;
};


typedef intr::member_hook<CSrvSocketTask,
                          TSrvSockListHook,
                          &CSrvSocketTask::m_SockListHook>  TSockListHookOpt;

typedef intr::list<CSrvSocketTask,
                   TSockListHookOpt,
                   intr::constant_time_size<false> >        TSockList;


class CSrvSocketFactory
{
public:
    CSrvSocketFactory(void);
    virtual ~CSrvSocketFactory(void);

    virtual CSrvSocketTask* CreateSocketTask(void) = 0;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_SOCKETS__HPP */
