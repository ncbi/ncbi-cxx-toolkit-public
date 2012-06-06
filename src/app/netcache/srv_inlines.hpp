#ifndef NETCACHE__SRV_INLINES__HPP
#define NETCACHE__SRV_INLINES__HPP
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


#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE



extern CSrvTime s_SrvStartTime;
extern CSrvTime s_LastJiffyTime;
extern TSrvThreadNum s_MaxRunningThreads;



enum EServerState {
    eSrvNotInitialized,
    eSrvInitialized,
    eSrvRunning,
    // Everything below is shutdown states
    eSrvShuttingDownSoft,
    eSrvShuttingDownHard,
    eSrvStopping,
    eSrvStopped
};

extern EServerState s_SrvState;



#ifdef NCBI_OS_LINUX

inline int
CSrvTime::CurSecs(void)
{
    return int(s_LastJiffyTime.tv_sec);
}

inline CSrvTime
CSrvTime::Current(void)
{
    CSrvTime result;
    clock_gettime(CLOCK_REALTIME, &result);
    return result;
}

inline
CSrvTime::CSrvTime(void)
{
    tv_sec = 0;
    tv_nsec = 0;
}

inline time_t&
CSrvTime::Sec(void)
{
    return tv_sec;
}

inline time_t
CSrvTime::Sec(void) const
{
    return tv_sec;
}

inline long&
CSrvTime::NSec(void)
{
    return tv_nsec;
}

inline long
CSrvTime::NSec(void) const
{
    return tv_nsec;
}

inline Uint8
CSrvTime::AsUSec(void) const
{
    return tv_sec * kUSecsPerSecond + tv_nsec / kNSecsPerUSec;
}

inline int
CSrvTime::Compare(const CSrvTime& t) const
{
    if (tv_sec < t.tv_sec)
        return -1;
    else if (tv_sec > t.tv_sec)
        return 1;
    else if (tv_nsec < t.tv_nsec)
        return -1;
    else if (tv_nsec > t.tv_nsec)
        return 1;
    return 0;
}

inline CSrvTime&
CSrvTime::operator+= (const CSrvTime& t)
{
    tv_sec += t.tv_sec;
    tv_nsec += t.tv_nsec;
    if (tv_nsec >= kNSecsPerSecond) {
        ++tv_sec;
        tv_nsec -= kNSecsPerSecond;
    }
    return *this;
}

inline CSrvTime&
CSrvTime::operator-= (const CSrvTime& t)
{
    tv_sec -= t.tv_sec;
    if (tv_nsec >= t.tv_nsec) {
        tv_nsec -= t.tv_nsec;
    }
    else {
        --tv_sec;
        tv_nsec += kNSecsPerSecond;
        tv_nsec -= t.tv_nsec;
    }
    return *this;
}

#else   /* NCBI_OS_LINUX */

inline int
CSrvTime::CurSecs(void)
{
    return 0;
}

inline CSrvTime
CSrvTime::Current(void)
{
    return CSrvTime();
}

inline
CSrvTime::CSrvTime(void)
{}

inline time_t&
CSrvTime::Sec(void)
{
    static time_t tt;
    return tt;
}

inline time_t
CSrvTime::Sec(void) const
{
    return 0;
}

inline long&
CSrvTime::NSec(void)
{
    static long tt;
    return tt;
}

inline long
CSrvTime::NSec(void) const
{
    return 0;
}

inline Uint8
CSrvTime::AsUSec(void) const
{
    return 0;
}

inline int
CSrvTime::Compare(const CSrvTime& t) const
{
    return 0;
}

inline CSrvTime&
CSrvTime::operator+= (const CSrvTime& t)
{
    return *this;
}

inline CSrvTime&
CSrvTime::operator-= (const CSrvTime& t)
{
    return *this;
}

#endif

inline bool
CSrvTime::operator> (const CSrvTime& t) const
{
    return Compare(t) > 0;
}

inline bool
CSrvTime::operator>= (const CSrvTime& t) const
{
    return Compare(t) >= 0;
}

inline bool
CSrvTime::operator< (const CSrvTime& t) const
{
    return Compare(t) < 0;
}

inline bool
CSrvTime::operator<= (const CSrvTime& t) const
{
    return Compare(t) <= 0;
}


inline CSrvTime
CTaskServer::GetStartTime(void)
{
    return s_SrvStartTime;
}

inline TSrvThreadNum
CTaskServer::GetMaxRunningThreads(void)
{
    return s_MaxRunningThreads;
}

inline bool
CTaskServer::IsRunning(void)
{
    return s_SrvState == eSrvRunning;
}

inline bool
CTaskServer::IsInShutdown(void)
{
    return s_SrvState >= eSrvShuttingDownSoft;
}

inline bool
CTaskServer::IsInSoftShutdown(void)
{
    return s_SrvState == eSrvShuttingDownSoft;
}

inline bool
CTaskServer::IsInHardShutdown(void)
{
    return s_SrvState >= eSrvShuttingDownHard;
}

inline bool
IsServerStopping(void)
{
    return s_SrvState >= eSrvStopping;
}


inline const CSrvDiagMsg&
CSrvDiagMsg::operator<< (const char* str) const
{
    return operator<< (CTempString(str));
}

inline const CSrvDiagMsg&
CSrvDiagMsg::operator<< (const string& str) const
{
    return operator<< (CTempString(str));
}

inline const CSrvDiagMsg&
CSrvDiagMsg::operator<< (Int4 num) const
{
    return operator<< (Int8(num));
}

inline const CSrvDiagMsg&
CSrvDiagMsg::operator<< (Uint4 num) const
{
    return operator<< (Uint8(num));
}

inline CSrvDiagMsg&
CSrvDiagMsg::PrintParam(CTempString name, Int4 value)
{
    return PrintParam(name, Int8(value));
}

inline CSrvDiagMsg&
CSrvDiagMsg::PrintParam(CTempString name, Uint4 value)
{
    return PrintParam(name, Uint8(value));
}


inline Uint1
CSrvTask::GetPriority(void)
{
    return m_Priority;
}

inline void
CSrvTask::SetPriority(Uint1 prty)
{
    m_Priority = prty;
}

inline CRequestContext*
CSrvTask::GetDiagCtx(void)
{
    return m_DiagCtx;
}


inline bool
CSrvSocketTask::IsReadDataAvailable(void)
{
    return m_RdSize > m_RdPos;
}

inline bool
CSrvSocketTask::IsWriteDataPending(void)
{
    return m_WrPos < m_WrSize;
}

inline bool
CSrvSocketTask::HasError(void)
{
    if (m_RegError) {
        x_PrintError();
        return true;
    }
    return false;
}

inline bool
CSrvSocketTask::CanHaveMoreRead(void)
{
    return m_SockCanReadMore;
}

inline bool
CSrvSocketTask::NeedToClose(void)
{
    return m_NeedToClose  ||  CTaskServer::IsInHardShutdown();
}

inline bool
CSrvSocketTask::NeedEarlyClose(void)
{
    return NeedToClose()  ||  HasError()  ||  !CanHaveMoreRead();
}

inline CSrvSocketTask&
CSrvSocketTask::WriteText(CTempString message)
{
    WriteData(message.data(), message.size());
    return *this;
}

inline CSrvSocketTask&
CSrvSocketTask::WriteText(const string& message)
{
    WriteData(message.data(), message.size());
    return *this;
}

inline CSrvSocketTask&
CSrvSocketTask::WriteText(const char* message)
{
    WriteData(message, strlen(message));
    return *this;
}

template <typename NumType>
inline CSrvSocketTask&
CSrvSocketTask::WriteNumber(NumType num)
{
    return WriteText(NStr::NumericToString(num));
}

inline void
CSrvSocketTask::RequestFlush(void)
{
    m_FlushIsDone = false;
    ACCESS_ONCE(m_NeedToFlush) = true;
    SetRunnable();
}

inline bool
CSrvSocketTask::FlushIsDone(void)
{
    return ACCESS_ONCE(m_FlushIsDone);
}

inline bool
CSrvSocketTask::ReadData(void* buf, Uint2 size)
{
    if (m_RdSize - m_RdPos < size) {
        ReadToBuf();
        if (m_RdSize - m_RdPos < size)
            return false;
    }
    memcpy(buf, m_RdBuf + m_RdPos, size);
    m_RdPos += size;
    return true;
}

template <typename NumType>
inline bool
CSrvSocketTask::ReadNumber(NumType* num)
{
    return ReadData(num, sizeof(*num));
}

inline bool
CSrvSocketTask::IsProxyInProgress(void)
{
    return m_ProxySrc  ||  m_ProxyDst;
}

inline bool
CSrvSocketTask::ProxyHadError(void)
{
    return m_ProxyHadError;
}

inline void
CSrvSocketTask::CloseSocket(void)
{
    x_CloseSocket(false);
}

inline void
CSrvSocketTask::AbortSocket(void)
{
    x_CloseSocket(true);
}


inline bool
CSrvTransitionTask::IsTransStateFinal(void)
{
    return m_TransState == eState_Final;
}


inline bool
CSrvTransConsumer::IsTransFinished(void)
{
    return m_TransFinished;
}


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_INLINES__HPP */
