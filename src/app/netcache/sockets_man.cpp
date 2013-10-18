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
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

#include <corelib/request_ctx.hpp>
#include <corelib/ncbireg.hpp>

#include "sockets_man.hpp"
#include "threads_man.hpp"
#include "srv_stat.hpp"

#ifdef NCBI_OS_LINUX
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <sys/epoll.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>

# ifndef EPOLLRDHUP
#   ifdef POLLRDHUP
#     define EPOLLRDHUP POLLRDHUP
#   else
#     define EPOLLRDHUP 0x2000
#   endif
# endif

#else
# define EPOLLIN      0x0001
# define EPOLLOUT     0x0004
# define EPOLLERR     0x0008
# define EPOLLHUP     0x0010
# define EPOLLRDHUP   0x2000
#endif


BEGIN_NCBI_SCOPE;


static const Uint1 kEpollEventsArraySize = 100;
/// 1000 below is chosen to be a little bit less than maximum packet size in
/// Ethernet (~1500 bytes).
static const Uint2 kSockReadBufSize = 1000;
static const Uint2 kSockMinWriteSize = 1000;
/// In calculations in the file it's assumed that kSockWriteBufSize is at least
/// twice as large as kSockMinWriteSize.
static const Uint2 kSockWriteBufSize = 2000;
/// 16 Uint4s on x86_64 is the size of CPU's cacheline. And it should be more
/// than enough for NetCache.
static const Uint1 kMaxCntListeningSocks = 16;


/// Per-thread structure containing information about sockets.
struct SSocketsData
{
    /// List of all open and not yet deleted sockets which were opened in this
    /// thread.
    TSockList sock_list;
    /// "Number of sockets" that this thread created/deleted. When new socket is
    /// created in this thread this number is increased by 1, when this thread
    /// deletes socket this number is decreased by 1. From time to time this
    /// number is added to the global variable of number of sockets. At that
    /// time 0 is written to sock_cnt to avoid adding the same number several
    /// times.
    Int2 sock_cnt;

    SSocketsData(void)
        : sock_cnt(0)
    {
    }
};


struct SListenSockInfo : public SSrvSocketInfo
{
    /// Index in the s_ListenSocks array.
    Uint1 index;
    /// Port to listen to.
    Uint2 port;
    /// File descriptor for the listening socket.
    int fd;
    /// Factory that will create CSrvSocketTask for each incoming socket.
    CSrvSocketFactory* factory;
};


class CSrvListener : public CSrvTask
{
public:
    CSrvListener(void);
    virtual ~CSrvListener(void);

    virtual void ExecuteSlice(TSrvThreadNum thread_idx);

public:
    /// Per-listening-socket numbers copied from s_ListenEvents when events are
    /// processed.
    Uint4 m_SeenEvents[kMaxCntListeningSocks];
    /// Per-listening-socket numbers copied from s_ListenErrors when errors are
    /// processed.
    Uint4 m_SeenErrors[kMaxCntListeningSocks];
};

static SListenSockInfo s_ListenSocks[kMaxCntListeningSocks];
static Uint1 s_CntListeningSocks = 0;
static CMiniMutex s_ListenSocksLock;
static Uint4 s_ListenEvents[kMaxCntListeningSocks];
static Uint4 s_ListenErrors[kMaxCntListeningSocks];
static CSrvListener s_Listener;
static int s_EpollFD = -1;
Uint4 s_TotalSockets = 0;
Uint2 s_SoftSocketLimit = 0;
Uint2 s_HardSocketLimit = 0;
static int s_SocketTimeout = 0;
static Uint1 s_OldSocksDelBatch = 0;
static Uint4 s_ConnTimeout = 10;
static string s_HostName;


extern Uint4 s_CurJiffies;
extern CSrvTime s_JiffyTime;
extern SSrvThread** s_Threads;




void
ConfigureSockets(CNcbiRegistry* reg, CTempString section)
{
    s_SoftSocketLimit = Uint2(reg->GetInt(section, "soft_sockets_limit", 1000));
    s_HardSocketLimit = Uint2(reg->GetInt(section, "hard_sockets_limit", 2000));
    s_ConnTimeout = Uint4(Uint8(reg->GetInt(section, "connection_timeout", 100))
                            * kNSecsPerMSec / s_JiffyTime.NSec());
    s_SocketTimeout = reg->GetInt(section, "min_socket_inactivity", 300);
    s_OldSocksDelBatch = Uint1(reg->GetInt(section, "sockets_cleaning_batch", 10));
    if (s_OldSocksDelBatch < 10)
        s_OldSocksDelBatch = 10;
}

void
AssignThreadSocks(SSrvThread* thr)
{
    SSocketsData* socks = new SSocketsData();
    thr->socks = socks;
}

void
ReleaseThreadSocks(SSrvThread* thr)
{}

static void
s_LogWithErrStr(CSrvDiagMsg::ESeverity severity,
                const char* err_msg,
                int x_errno,
                const char* errno_str,
                const char* file,
                int line,
                const char* func)
{
    if (CSrvDiagMsg::IsSeverityVisible(severity)) {
        CSrvDiagMsg().StartSrvLog(severity, file, line, func)
                << err_msg << ", errno=" << x_errno << " (" << errno_str << ")";
    }
    GetCurThread()->stat->ErrorOnSocket();
}

static void
s_LogWithErrno(CSrvDiagMsg::ESeverity severity,
               const char* err_msg,
               int x_errno,
               const char* file,
               int line,
               const char* func)
{
    s_LogWithErrStr(severity, err_msg, x_errno, strerror(x_errno), file, line, func);
}

#define LOG_WITH_ERRNO(sev, msg, x_errno)                       \
    s_LogWithErrno(CSrvDiagMsg::sev, msg, x_errno,              \
                   __FILE__, __LINE__, NCBI_CURRENT_FUNCTION)   \
/**/

#ifdef NCBI_OS_LINUX

static void
s_LogWithAIErr(CSrvDiagMsg::ESeverity severity,
               const char* err_msg,
               int x_aierr,
               const char* file,
               int line,
               const char* func)
{
    s_LogWithErrStr(severity, err_msg, x_aierr, gai_strerror(x_aierr), file, line, func);
}

#define LOG_WITH_AIERR(sev, msg, x_aierr)                       \
    s_LogWithAIErr(CSrvDiagMsg::sev, msg, x_aierr,              \
                   __FILE__, __LINE__, NCBI_CURRENT_FUNCTION)   \
/**/

#endif

static inline bool
s_SetSocketNonBlock(int sock)
{
#ifdef NCBI_OS_LINUX
    int res = fcntl(sock, F_SETFL, O_NONBLOCK);
    if (res) {
        LOG_WITH_ERRNO(Critical, "Cannot set socket non-blocking", errno);
        return false;
    }
#endif
    return true;
}

static bool
s_SetSocketOptions(int sock)
{
#ifdef NCBI_OS_LINUX
    int value = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
    if (res) {
        LOG_WITH_ERRNO(Critical, "Cannot set socket's keep-alive property", errno);
        return false;
    }
    res = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
    if (res) {
        LOG_WITH_ERRNO(Critical, "Cannot set socket's no-delay property", errno);
        return false;
    }
#endif
    return true;
}

static void
s_SetSocketQuickAck(int sock)
{
#ifdef NCBI_OS_LINUX
    int value = 1;
    int res = setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &value, sizeof(value));
    if (res)
        LOG_WITH_ERRNO(Critical, "Cannot set socket's quick-ack property", errno);
#endif
}

static bool
s_CreateListeningSocket(Uint1 idx)
{
    SListenSockInfo& sock_info = s_ListenSocks[idx];
#ifdef NCBI_OS_LINUX
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        LOG_WITH_ERRNO(Critical, "Cannot create socket", errno);
        return false;
    }
    if (!s_SetSocketNonBlock(sock)) {
        close(sock);
        return false;
    }

    int value = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
    if (res)
        LOG_WITH_ERRNO(Error, "Cannot set socket's reuse-address property", errno);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(sock_info.port);
    res = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        string err_msg("Cannot bind socket to port ");
        err_msg += NStr::NumericToString(sock_info.port);
        LOG_WITH_ERRNO(Critical, err_msg.c_str(), errno);
        close(sock);
        return false;
    }
    res = listen(sock, 128);
    if (res) {
        LOG_WITH_ERRNO(Critical, "Cannot listen on a socket", errno);
        close(sock);
        return false;
    }

    struct epoll_event evt;
    evt.events = EPOLLIN | EPOLLET;
    evt.data.ptr = (void*)&sock_info;
    res = epoll_ctl(s_EpollFD, EPOLL_CTL_ADD, sock, &evt);
    if (res) {
        LOG_WITH_ERRNO(Critical, "Cannot add listening socket to epoll", errno);
        close(sock);
        return false;
    }

    sock_info.fd = sock;
#endif
    return true;
}

static bool
s_StartListening(void)
{
    for (Uint1 i = 0; i < s_CntListeningSocks; ++i) {
        if (!s_CreateListeningSocket(i))
            return false;
    }
    return true;
}

static inline void
s_RegisterListenEvent(SListenSockInfo* sock_info, Uint4 event)
{
    if (event & EPOLLIN)
        ++s_ListenEvents[sock_info->index];
    else if (event & (EPOLLERR + EPOLLHUP))
        ++s_ListenErrors[sock_info->index];
    s_Listener.SetRunnable();
}

static inline void
s_RegisterClientEvent(CSrvSocketTask* task, Uint4 event)
{
    if ((event & EPOLLIN)  &&  task->m_SeenReadEvts == task->m_RegReadEvts)
        ++task->m_RegReadEvts;
    if ((event & EPOLLOUT)  &&  task->m_SeenWriteEvts == task->m_RegWriteEvts)
        ++task->m_RegWriteEvts;
    if (event & EPOLLRDHUP)
        task->m_RegReadHup = true;
    if (event & (EPOLLERR + EPOLLHUP))
        task->m_RegError = true;
    task->SetRunnable();
}

static void
s_LogSocketError(CSrvDiagMsg::ESeverity severity,
                 int fd,
                 const char* prefix,
                 const char* file,
                 int line,
                 const char* func)
{
#ifdef NCBI_OS_LINUX
    int x_errno = 0;
    socklen_t x_size = sizeof(x_errno);
    int res = getsockopt(fd, SOL_SOCKET, SO_ERROR, &x_errno, &x_size);
    if (res)
        x_errno = errno;
    if (x_errno)
        s_LogWithErrno(severity, prefix, x_errno, file, line, func);
#endif
}

#define LOG_SOCK_ERROR(sev, fd, prefix)   \
    s_LogSocketError(CSrvDiagMsg::sev, fd, prefix, __FILE__, __LINE__, NCBI_CURRENT_FUNCTION)

static void
s_CloseSocket(int fd, bool do_abort)
{
#ifdef NCBI_OS_LINUX
    int res;

    if (do_abort) {
        struct linger lgr;
        lgr.l_linger = 0;
        lgr.l_onoff  = 1;
        res = setsockopt(fd, SOL_SOCKET, SO_LINGER, (void*)&lgr, sizeof(lgr));
        if (res)
            LOG_WITH_ERRNO(Critical, "Error setting so_linger", errno);
    }

    int val = -1;
    res = setsockopt(fd, IPPROTO_TCP, TCP_LINGER2, (void*)&val, sizeof(val));
    if (res)
        LOG_WITH_ERRNO(Critical, "Error setting tcp_linger2", errno);

    int x_errno = 0;
    do {
        res = close(fd);
    }
    while (res  &&  (x_errno = errno) == EINTR);
    if (res)
        LOG_WITH_ERRNO(Critical, "Error closing socket", x_errno);
#endif
}

static void
s_CleanSockResources(CSrvSocketTask* task)
{
    task->m_Fd = -1;
    CRequestContext* ctx = task->GetDiagCtx();
    ctx->SetBytesRd(task->m_ReadBytes);
    ctx->SetBytesWr(task->m_WrittenBytes);
    Uint8 open_time = Uint8(ctx->GetRequestTimer().Elapsed() * kUSecsPerSecond);
    GetCurThread()->stat->SockClose(ctx->GetRequestStatus(), open_time);
    CSrvDiagMsg().StopRequest(ctx);
    task->ReleaseDiagCtx();
    SSrvThread* thr = GetCurThread();
    --thr->socks->sock_cnt;
}

string
CTaskServer::IPToString(Uint4 ip)
{
    char buf[20];
#ifdef NCBI_OS_LINUX
    Uint1* hb = (Uint1*)&ip;
    snprintf(buf, 20, "%u.%u.%u.%u", hb[0], hb[1], hb[2], hb[3]);
#endif
    return buf;
}

string
CTaskServer::GetHostByIP(Uint4 ip)
{
    char buf[256];
#ifdef NCBI_OS_LINUX
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    int x_errno = getnameinfo((struct sockaddr*)&addr, sizeof(addr),
                              buf, sizeof(buf), NULL, 0, NI_NAMEREQD | NI_NOFQDN);
    if (x_errno != 0) {
        LOG_WITH_AIERR(Critical, "Error from getnameinfo", x_errno);
        buf[0] = 0;
    }
#endif
    return buf;
}

const string&
CTaskServer::GetHostName(void)
{
    if (s_HostName.empty()) {
#ifdef NCBI_OS_LINUX
        char buf[256];
        if (gethostname(buf, sizeof(buf)))
            buf[0] = 0;
        s_HostName = buf;
#endif
    }
    return s_HostName;
}

Uint4
CTaskServer::GetIPByHost(const string& host)
{
    Uint4 ip = 0;
#ifdef NCBI_OS_LINUX
    ip = inet_addr(host.c_str());
    if (ip == INADDR_NONE) {
        struct addrinfo in_addr;
        memset(&in_addr, 0, sizeof(in_addr));
        in_addr.ai_family = AF_INET;
        struct addrinfo* out_addr = NULL;
        int x_errno = getaddrinfo(host.c_str(), NULL, &in_addr, &out_addr);
        if (x_errno) {
            LOG_WITH_AIERR(Critical, "Error from getaddrinfo", x_errno);
            ip = 0;
        }
        else
            ip = ((struct sockaddr_in*)out_addr->ai_addr)->sin_addr.s_addr;
        freeaddrinfo(out_addr);
    }
#endif
    return ip;
}

static void
s_CreateDiagRequest(CSrvSocketTask* task, Uint2 port, Uint4 phost, Uint2 pport)
{
    task->CreateNewDiagCtx();
    task->GetDiagCtx()->SetClientIP(CTaskServer::IPToString(phost));
    task->m_ConnReqId = NStr::UInt8ToString(task->GetDiagCtx()->GetRequestID());

    CSrvDiagMsg().StartRequest(task->GetDiagCtx())
                 .PrintParam("_type", "conn")
                 .PrintParam("pport", pport)
                 .PrintParam("port", port)
                 .PrintParam("conn", task->m_ConnReqId);

    task->m_ReadBytes = task->m_WrittenBytes = 0;
}

static void
s_ProcessListenError(Uint1 sock_idx)
{
    s_Listener.m_SeenErrors[sock_idx] = s_ListenErrors[sock_idx];

    SListenSockInfo& sock_info = s_ListenSocks[sock_idx];
    LOG_SOCK_ERROR(Critical, sock_info.fd, "Error in listening socket");
// try to reopen
    s_CloseSocket(sock_info.fd, true);
    s_CreateListeningSocket(sock_idx);
}

static void
s_ProcessListenEvent(Uint1 sock_idx, TSrvThreadNum thread_num)
{
    s_Listener.m_SeenEvents[sock_idx] = s_ListenEvents[sock_idx];
    SListenSockInfo& sock_info = s_ListenSocks[sock_idx];
    for (;;) {
#ifdef NCBI_OS_LINUX
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int new_sock = accept(sock_info.fd, (struct sockaddr*)&addr, &len);
        if (new_sock == -1) {
            int x_errno = errno;
            if (x_errno != EAGAIN  &&  x_errno != EWOULDBLOCK) {
                LOG_WITH_ERRNO(Critical, "Error accepting new socket", x_errno);
                s_CloseSocket(sock_info.fd, true);
                s_CreateListeningSocket(sock_idx);
            }
            break;
        }
        if (s_TotalSockets >= s_HardSocketLimit) {
            SRV_LOG(Error, "Number of open sockets " << s_TotalSockets
                           << " is bigger than hard limit " << s_HardSocketLimit
                           << ". Rejecting new connection.");
            s_CloseSocket(new_sock, true);
            continue;
        }
        if (!s_SetSocketNonBlock(new_sock)  ||  !s_SetSocketOptions(new_sock)) {
            s_CloseSocket(new_sock, true);
            continue;
        }

        CSrvSocketTask* task = sock_info.factory->CreateSocketTask();
        task->m_Fd = new_sock;
        task->m_PeerAddr = addr.sin_addr.s_addr;
        task->m_PeerPort = addr.sin_port;
        s_CreateDiagRequest(task, s_ListenSocks[sock_idx].port,
                            task->m_PeerAddr, task->m_PeerPort);
        s_Threads[thread_num]->stat->SockOpenPassive();
        if (!task->StartProcessing(thread_num))
            task->Terminate();
#endif
    }
}

void
DoSocketWait(void)
{
#ifdef NCBI_OS_LINUX
    CSrvTime wait_time = s_JiffyTime;
    Uint4 wait_msec = wait_time.NSec() / 1000000;
    if (wait_msec == 0)
        wait_msec = 1;
    struct epoll_event events[kEpollEventsArraySize];
    int res = epoll_wait(s_EpollFD, events, kEpollEventsArraySize, wait_msec);
    if (res < 0) {
        int x_errno = errno;
        if (x_errno != EINTR)
            LOG_WITH_ERRNO(Critical, "Error in epoll_wait", x_errno);
    }
    for (int i = 0; i < res; ++i) {
        struct epoll_event& evt = events[i];
        SSrvSocketInfo* sock_info = (SSrvSocketInfo*)evt.data.ptr;
        if (sock_info->is_listening)
            s_RegisterListenEvent((SListenSockInfo*)sock_info, evt.events);
        else
            s_RegisterClientEvent((CSrvSocketTask*)sock_info, evt.events);
    }
#endif
}

bool
InitSocketsMan(void)
{
#ifdef NCBI_OS_LINUX
    s_EpollFD = epoll_create(1);
    if (s_EpollFD == -1) {
        LOG_WITH_ERRNO(Critical, "Cannot create epoll descriptor", errno);
        return false;
    }
#endif

    if (CTaskServer::GetHostName().empty()) {
        LOG_WITH_ERRNO(Critical, "Error in gethostname", errno);
        return false;
    }
    return true;
}

bool
StartSocketsMan(void)
{
    if (s_CntListeningSocks == 0) {
        SRV_LOG(Critical, "There's no listening sockets, shutting down");
        return false;
    }
    if (!s_StartListening())
        return false;
    return true;
}

void
FinalizeSocketsMan(void)
{
#ifdef NCBI_OS_LINUX
    close(s_EpollFD);
#endif
}

static void
s_SaveSocket(CSrvSocketTask* task)
{
    SSrvThread* thr = GetCurThread();
    SSocketsData* socks = thr->socks;
    socks->sock_list.push_front(*task);
    ++socks->sock_cnt;

    task->m_LastActive = CSrvTime::CurSecs();
}

void
s_DeleteOldestSockets(TSockList& lst)
{
#ifdef NCBI_COMPILER_GCC
    CSrvSocketTask* old_socks[s_OldSocksDelBatch];
    int old_active[s_OldSocksDelBatch];
#else
    CSrvSocketTask* old_socks[100];
    int old_active[100];
#endif

    memset(old_socks, 0, sizeof(old_socks));
    memset(old_active, 0, sizeof(old_active));

    // Search in the socket list sockets that were used least recently and
    // that were not used for at least s_SocketTimeout seconds.
    int limit_time = CSrvTime::CurSecs() - s_SocketTimeout;
    Uint1 cnt_old = 0;
    NON_CONST_ITERATE(TSockList, it, lst) {
        CSrvSocketTask* task = &*it;
        int active = task->m_LastActive;
        if (active >= limit_time)
            continue;

        // Binary search to find the place where to put new socket in our sorted
        // list of candidates for closing.
        Uint1 low = 0, high = cnt_old;
        while (high > low) {
            Uint1 mid = (high + low) / 2;
            if (old_active[mid] > active)
                high = mid;
            else
                low = mid + 1;
        }
        if (low >= cnt_old  &&  cnt_old == s_OldSocksDelBatch)
            continue;
        if (cnt_old == s_OldSocksDelBatch)
            --cnt_old;
        if (low < cnt_old) {
            memmove(&old_socks[low + 1], &old_socks[low],
                    (cnt_old - low) * sizeof(old_socks[0]));
            memmove(&old_active[low + 1], &old_active[low],
                    (cnt_old - low) * sizeof(old_active[0]));
        }
        old_socks[low] = task;
        old_active[low] = active;
        ++cnt_old;
    }

    for (Uint1 i = 0; i < cnt_old; ++i) {
        CSrvSocketTask* task = old_socks[i];
        if (task->m_LastActive < limit_time) {
            // We cannot physically close here not only because it can race with
            // socket actually starting to do something but also because it can
            // need to do some finalization before it will be possible to close
            // and delete it.
            task->m_NeedToClose = true;
            task->SetRunnable();
        }
    }
}

void
MoveAllSockets(SSocketsData* dst_socks, SSocketsData* src_socks)
{
    // Move all sockets from src_socks to dst_socks.
    dst_socks->sock_list.splice(dst_socks->sock_list.begin(), src_socks->sock_list);
    dst_socks->sock_cnt += src_socks->sock_cnt;
    src_socks->sock_cnt = 0;
}

void
PromoteSockAmount(SSocketsData* socks)
{
    AtomicAdd(s_TotalSockets, socks->sock_cnt);
    socks->sock_cnt = 0;
}

void
CheckConnectsTimeout(SSocketsData* socks)
{
    TSockList& lst = socks->sock_list;
    NON_CONST_ITERATE(TSockList, it, lst) {
        CSrvSocketTask* task = &*it;
        if (task->m_ConnStartJfy != 0
            &&  s_CurJiffies - task->m_ConnStartJfy > s_ConnTimeout
            &&  task->m_RegWriteEvts == task->m_SeenWriteEvts
            &&  !task->m_RegError)
        {
            task->m_RegError = true;
            task->SetRunnable();
        }
    }
}

void
CleanSocketList(SSocketsData* socks)
{
    // Terminate all sockets which have their Terminate() method called already.
    TSockList& lst = socks->sock_list;
    ERASE_ITERATE(TSockList, it, lst) {
        CSrvSocketTask* task = &*it;
        if (task->m_TaskFlags & fTaskNeedTermination) {
            lst.erase(it);
            MarkTaskTerminated(task);
        }
    }

    // Also ask some unused sockets to close if necessary
    if (s_TotalSockets >= s_SoftSocketLimit)
        s_DeleteOldestSockets(lst);
}

void
SetAllSocksRunnable(SSocketsData* socks)
{
    TSockList& lst = socks->sock_list;
    NON_CONST_ITERATE(TSockList, it, lst) {
        it->SetRunnable();
    }
}

void
RequestStopListening(void)
{
    s_Listener.SetRunnable();
}

static size_t
s_ReadFromSocket(CSrvSocketTask* task, void* buf, size_t size)
{
    if (!task->m_SockHasRead  &&  task->m_SeenReadEvts == task->m_RegReadEvts)
        return 0;
    if (size == 0)
        return 0;

    ssize_t n_read = 0;
#ifdef NCBI_OS_LINUX
retry:
    n_read = recv(task->m_Fd, buf, size, 0);
    if (n_read == -1) {
        int x_errno = errno;
        if (x_errno == EINTR)
            goto retry;
        // We should have returned at the very top but due to some races
        // we fell down here. Anyway we should avoid changing variables
        // at the bottom.
        if (x_errno == EWOULDBLOCK) {
            return 0;
        }
        if (x_errno == EAGAIN) {
            task->m_SeenReadEvts = task->m_RegReadEvts;
            return 0;
        }
        LOG_WITH_ERRNO(Warning, "Error reading from socket", x_errno);
        task->m_RegError = true;
        n_read = 0;
    }
#endif
    task->m_ReadBytes += n_read;
    task->m_SockHasRead = size_t(n_read) == size;
    task->m_SockCanReadMore = n_read != 0
                              &&  (task->m_SockHasRead  ||  !task->m_RegReadHup);

    return size_t(n_read);
}

static size_t
s_WriteToSocket(CSrvSocketTask* task, const void* buf, size_t size)
{
    if (!task->m_SockCanWrite  &&  task->m_SeenWriteEvts == task->m_RegWriteEvts)
        return 0;
    if (size == 0)
        return 0;

    task->m_SeenWriteEvts = task->m_RegWriteEvts;
    ssize_t n_written = 0;
#ifdef NCBI_OS_LINUX
retry:
    n_written = send(task->m_Fd, buf, size, 0);
    if (n_written == -1) {
        int x_errno = errno;
        if (x_errno == EINTR)
            goto retry;
        if (x_errno == EAGAIN  ||  x_errno == EWOULDBLOCK) {
            // We should have returned at the very top but due to some races
            // we fell down here. Anyway we should avoid changing variables
            // at the bottom.
            return 0;
        }
        LOG_WITH_ERRNO(Warning, "Error writing to socket", x_errno);
        task->m_RegError = true;
        n_written = 0;
    }
#endif
    task->m_WrittenBytes += n_written;
    task->m_SockCanWrite = size_t(n_written) == size;

    return size_t(n_written);
}

static inline void
s_CompactBuffer(char* buf, Uint2& size, Uint2& pos)
{
    if (pos != 0) {
        if (pos > size)
            abort();
        Uint2 new_size = size - pos;
        memmove(buf, buf + pos, new_size);
        size = new_size;
        pos = 0;
    }
}

static inline void
s_ReadLF(CSrvSocketTask* task)
{
    if (task->m_RdPos < task->m_RdSize) {
        char c = task->m_RdBuf[task->m_RdPos];
        if (c == '\n'  ||  c == '\0') {
            ++task->m_RdPos;
            task->m_CRMet = false;
        }
    }
}

static inline Uint2
s_ReadFromBuffer(CSrvSocketTask* task, void* dest, size_t size)
{
    Uint2 to_copy = task->m_RdSize - task->m_RdPos;
    if (size < to_copy)
        to_copy = Uint2(size);
    memcpy(dest, task->m_RdBuf + task->m_RdPos, to_copy);
    task->m_RdPos += to_copy;
    return to_copy;
}

static inline void
s_CopyData(CSrvSocketTask* task, const void* buf, Uint2 size)
{
    memcpy(task->m_WrBuf + task->m_WrSize, buf, size);
    task->m_WrSize += size;
}

static inline size_t
s_WriteNoPending(CSrvSocketTask* task, const void* buf, size_t size)
{
    if (size < kSockMinWriteSize) {
        s_CopyData(task, buf, Uint2(size));
        return size;
    }
    else {
        return s_WriteToSocket(task, buf, size);
    }
}

static inline void
s_FlushData(CSrvSocketTask* task)
{
    if (task->m_WrSize < task->m_WrPos)
        abort();
    Uint2 n_written = Uint2(s_WriteToSocket(task,
                                            task->m_WrBuf + task->m_WrPos,
                                            task->m_WrSize - task->m_WrPos));
    task->m_WrPos += n_written;
}

static inline void
s_CompactWrBuffer(CSrvSocketTask* task)
{
    s_CompactBuffer(task->m_WrBuf, task->m_WrSize, task->m_WrPos);
}

static void
s_DoDataProxy(CSrvSocketTask* src)
{
    CSrvSocketTask* dst = src->m_ProxyDst;
    Uint8& size = src->m_ProxySize;

    if (src->NeedEarlyClose()  ||  dst->NeedEarlyClose())
        goto finish_with_error;

    while (size != 0) {
        if (src->m_RdPos < src->m_RdSize) {
            // If there's something in src's read buffer we should copy it first.
            Uint2 to_write = src->m_RdSize - src->m_RdPos;
            if (to_write > size)
                to_write = Uint2(size);
            // We call Write() so that it could figure out by itself whether
            // the new data should go to dst's write buffer, or to socket directly,
            // or some combination of that.
            Uint2 n_done = Uint2(dst->Write(src->m_RdBuf + src->m_RdPos, to_write));
            size -= n_done;
            src->m_RdPos += n_done;
            if (dst->NeedEarlyClose())
                goto finish_with_error;
            if (n_done < to_write) {
                // If there's still something left in src's read buffer then return
                // now.
                return;
            }
            continue;
        }
        // Read buffer in src is empty, we'll need to read directly from src's
        // socket. Now let's see how much we should read from there.
        Uint2 to_read = dst->m_WrMemSize - dst->m_WrSize;
        if (to_read == 0) {
            // Write buffer in dst is full, we need to flush it first.
            s_FlushData(dst);
            if (dst->NeedEarlyClose())
                goto finish_with_error;
            s_CompactWrBuffer(dst);
            to_read = dst->m_WrMemSize - dst->m_WrSize;
            if (to_read == 0) {
                // If nothing was flushed we can't continue further.
                return;
            }
        }
        if (to_read > size)
            to_read = Uint2(size);

        Uint2 n_done;
        if (to_read < kSockReadBufSize) {
            // If very small amount is needed (either because nothing else should
            // be proxied or because the rest of write buffer in dst is filled)
            // then we better read into src's read buffer first and then copy
            // whatever is necessary into dst's write buffer.
            src->ReadToBuf();
            if (src->NeedEarlyClose())
                goto finish_with_error;
            _ASSERT(src->m_RdPos == 0);
            n_done = src->m_RdSize;
            if (n_done > to_read)
                n_done = to_read;
            memcpy(dst->m_WrBuf + dst->m_WrSize, src->m_RdBuf, n_done);
            src->m_RdPos = n_done;
        }
        else {
            // If amount we need is pretty big we'll read directly from src's
            // socket into dst's write buffer. And later dst's write buffer will
            // be flushed into dst's socket.
            n_done = Uint2(s_ReadFromSocket(src, dst->m_WrBuf + dst->m_WrSize,
                                            to_read));
            if (src->NeedEarlyClose())
                goto finish_with_error;
        }
        if (n_done == 0) {
            // If nothing was copied in the above if/else then we should return
            // and wait when more data will be available in src's socket.
            return;
        }

        dst->m_WrSize += n_done;
        size -= n_done;
        if (dst->m_WrSize >= kSockMinWriteSize) {
            // If dst's write buffer is already filled enough we can flush data
            // into the socket right now.
            s_FlushData(dst);
            if (dst->NeedEarlyClose())
                goto finish_with_error;
            s_CompactWrBuffer(dst);
        }
    }
    goto finish_proxy;

finish_with_error:
    src->m_ProxyHadError = true;
    dst->m_ProxyHadError = true;

finish_proxy:
    src->m_ProxyDst = NULL;
    dst->m_ProxySrc = NULL;
    dst->SetRunnable();
}




bool
CTaskServer::AddListeningPort(Uint2 port, CSrvSocketFactory* factory)
{
    bool result = false;
    s_ListenSocksLock.Lock();
    Uint1 idx = s_CntListeningSocks;
    if (idx == kMaxCntListeningSocks)
        goto unlock_and_exit;

    SListenSockInfo* sock_info;
    sock_info = &s_ListenSocks[idx];
    sock_info->is_listening = true;
    sock_info->index = idx;
    sock_info->port = port;
    sock_info->factory = factory;
    ACCESS_ONCE(s_CntListeningSocks) = idx + 1;

    if (!CTaskServer::IsRunning()  ||  s_CreateListeningSocket(idx))
        result = true;

unlock_and_exit:
    s_ListenSocksLock.Unlock();
    return result;
}


CSrvListener::CSrvListener(void)
{}

CSrvListener::~CSrvListener(void)
{}

void
CSrvListener::ExecuteSlice(TSrvThreadNum thread_idx)
{
// process events added by main thread
    Uint1 cnt_listen = ACCESS_ONCE(s_CntListeningSocks);
    for (Uint1 i = 0; i < cnt_listen; ++i) {
        if (m_SeenErrors[i] != s_ListenErrors[i])
            s_ProcessListenError(i);
        if (m_SeenEvents[i] != s_ListenEvents[i])
            s_ProcessListenEvent(i, thread_idx);

        if (CTaskServer::IsInShutdown()) {
            SListenSockInfo& sock_info = s_ListenSocks[i];
            if (sock_info.fd != -1)
                s_CloseSocket(sock_info.fd, false);
            sock_info.fd = -1;
        }
    }
}


CSrvSocketTask::CSrvSocketTask(void)
    : m_ProxySrc(NULL),
      m_ProxyDst(NULL),
      m_ConnStartJfy(0),
      m_LastActive(0),
      m_Fd(-1),
      m_RdSize(0),
      m_RdPos(0),
      m_WrMemSize(kSockWriteBufSize),
      m_WrSize(0),
      m_WrPos(0),
      m_CRMet(false),
      m_ProxyHadError(false),
      m_SockHasRead(false),
      m_SockCanWrite(false),
      m_SockCanReadMore(true),
      m_NeedToClose(false),
      m_NeedToFlush(false),
      m_SeenReadEvts(0),
      m_SeenWriteEvts(0),
      m_RegReadEvts(0),
      m_RegWriteEvts(0),
      m_RegReadHup(false),
      m_RegError(false),
      m_ErrorPrinted(false)
{
    is_listening = false;
    m_RdBuf = (char*)malloc(kSockReadBufSize);
    m_WrBuf = (char*)malloc(kSockWriteBufSize);
}

CSrvSocketTask::~CSrvSocketTask(void)
{
    if (m_Fd != -1)
        abort();
    free(m_RdBuf);
    free(m_WrBuf);
}

void
CSrvSocketTask::x_PrintError(void)
{
    if (!m_ErrorPrinted) {
        LOG_SOCK_ERROR(Warning, m_Fd, "Error in the socket");
        m_ErrorPrinted = true;
    }
}

bool
CSrvSocketTask::ReadToBuf(void)
{
    s_CompactBuffer(m_RdBuf, m_RdSize, m_RdPos);
    Uint2 n_read = Uint2(s_ReadFromSocket(this, m_RdBuf + m_RdSize,
                                          kSockReadBufSize - m_RdSize));
    m_RdSize += n_read;
    if (m_CRMet)
        s_ReadLF(this);
    return m_RdSize > 0;
}

bool
CSrvSocketTask::ReadLine(CTempString* line)
{
    if (!ReadToBuf())
        return false;

    Uint2 crlf_pos;
    for (crlf_pos = m_RdPos; crlf_pos < m_RdSize; ++crlf_pos) {
        char c = m_RdBuf[crlf_pos];
        if (c == '\n'  ||  c == '\r'  ||  c == '\0')
            break;
    }
    if (crlf_pos >= m_RdSize) {
        if (m_RdSize == kSockReadBufSize) {
            SRV_LOG(Critical, "Too long line in the protocol - at least "
                              << m_RdSize << " bytes");
            m_RegError = true;
        }
        return false;
    }

    line->assign(m_RdBuf + m_RdPos, crlf_pos - m_RdPos);
    if (m_RdBuf[crlf_pos] == '\r') {
        m_CRMet = true;
        ++crlf_pos;
    }
    m_RdPos = crlf_pos;
    s_ReadLF(this);
    return true;
}

size_t
CSrvSocketTask::Read(void* buf, size_t size)
{
    size_t n_read = 0;
    if (size != 0  &&  m_RdPos < m_RdSize) {
        Uint2 copied = s_ReadFromBuffer(this, buf, size);
        n_read += copied;
        buf = (char*)buf + copied;
        size -= copied;
    }
    if (size == 0)
        return n_read;

    if (size < kSockReadBufSize) {
        if (ReadToBuf())
            n_read += s_ReadFromBuffer(this, buf, size);
    }
    else {
        n_read += s_ReadFromSocket(this, buf, size);
    }
    return n_read;
}

size_t
CSrvSocketTask::Write(const void* buf, size_t size)
{
    Uint2 has_size = m_WrSize - m_WrPos;
    if (has_size == 0) {
        // If there's nothing in our write buffer then we either copy given data
        // into write buffer or (if it's too much of data) write directly into
        // socket.
        return s_WriteNoPending(this, buf, size);
    }
    else if (has_size + size <= kSockWriteBufSize) {
        // If write buffer has room for the new data then just copy data into
        // the write buffer.
        s_CompactWrBuffer(this);
        s_CopyData(this, buf, Uint2(size));
        return size;
    }
    else if (has_size < kSockMinWriteSize) {
        // If write buffer doesn't have enough room to fit the new data and
        // amount of data it already has is too small then we copy part of the new
        // data to fulfill minimum requirements and then write the rest to the
        // socket directly, because it's guaranteed that the rest will be more
        // than kSockMinWriteSize (see comment to kSockWriteBufSize - it's guaranteed
        // that kSockWriteBufSize is at least 2 times bigger than kSockMinWriteSize).
        Uint2 to_copy = kSockMinWriteSize - has_size;
        s_CompactWrBuffer(this);
        s_CopyData(this, buf, to_copy);
        s_FlushData(this);
        if (IsWriteDataPending())
            return to_copy;
        s_CompactWrBuffer(this);

        buf = (const char*)buf + to_copy;
        size -= to_copy;
        size_t n_written = s_WriteToSocket(this, buf, size);
        return to_copy + n_written;
    }
    else {
        // If write buffer already has enough data to fulfill requirement of minimum
        // amount to write to socket then we just flush it and then we are back
        // to the very first "if" above when we don't have anything in our
        // write buffer.
        s_FlushData(this);
        if (IsWriteDataPending())
            return 0;
        s_CompactWrBuffer(this);
        return s_WriteNoPending(this, buf, size);
    }
}

void
CSrvSocketTask::WriteData(const void* buf, size_t size)
{
    if (Uint2(numeric_limits<Uint2>::max() - m_WrSize) < size)
        abort();
    if (m_WrSize + size > m_WrMemSize) {
        m_WrBuf = (char*)realloc(m_WrBuf, m_WrSize + size);
        m_WrMemSize = Uint2(m_WrSize + size);
    }
    s_CopyData(this, buf, Uint2(size));
}

void
CSrvSocketTask::Flush(void)
{
    if (HasError()  ||  !IsWriteDataPending())
        return;

    s_FlushData(this);
    if (!IsWriteDataPending())
        s_CompactWrBuffer(this);
    else
        m_NeedToFlush = true;
}

void
CSrvSocketTask::StartProxyTo(CSrvSocketTask* dst_task,
                             Uint8 proxy_size)
{
    m_ProxyDst = dst_task;
    m_ProxySize = proxy_size;
    m_ProxyHadError = false;
    dst_task->m_ProxyHadError = false;
    dst_task->m_ProxySrc = this;
    s_DoDataProxy(this);
}

void
CSrvSocketTask::InternalRunSlice(TSrvThreadNum thr_num)
{
// remember last activity time stamp (to close inactive ones later on)
    m_LastActive = CSrvTime::CurSecs();

// if just connected, check for timeout
    if (m_ConnStartJfy != 0) {
        if (m_RegWriteEvts != m_SeenWriteEvts)
            m_ConnStartJfy = 0;
        else if (s_CurJiffies - m_ConnStartJfy > s_ConnTimeout) {
            m_RegError = true;
            m_ConnStartJfy = 0;
            SRV_LOG(Warning, "Connection has timed out");
        }
    }

    if (m_ProxyDst) {
// to just forward data
        m_ProxyDst->m_LastActive = CSrvTime::CurSecs();
        s_DoDataProxy(this);
        if (!m_ProxyDst)
            ExecuteSlice(thr_num);
    }
    else {
        CSrvSocketTask* src = ACCESS_ONCE(m_ProxySrc);
        if (src) {
            // Proxying is in progress and by design all proxying should be done
            // by source socket. So we just transfer this event to it -- probably
            // we just became writable and this proxying will be able to continue.
            src->SetRunnable();
        }
        else if (!m_NeedToFlush) {
            // In the most often "standard" situation (when there's no proxying
            // involved and there's no need to flush write buffers) we will end up
            // here and call code from derived class.
            ExecuteSlice(thr_num);
        }
        else {
            s_FlushData(this);
            if (!IsWriteDataPending()  ||  NeedEarlyClose()) {
                // If this socket was already closed by client or server should
                // urgently shutdown then we don't care about lost data in the
                // write buffers. Otherwise we'll be here when all data has been
                // flushed and thus it's time for code in the derived class
                // to execute.
                s_CompactWrBuffer(this);
                m_NeedToFlush = false;
                ACCESS_ONCE(m_FlushIsDone) = true;
                ExecuteSlice(thr_num);
            }
        }
    }

    if (m_RegReadEvts == m_SeenReadEvts  &&  !m_SockHasRead  &&  m_SockCanReadMore
        &&  m_Fd != -1)
    {
        // Ask kernel to give us data from client as quickly as possible.
        s_SetSocketQuickAck(m_Fd);
    }

    m_LastActive = CSrvTime::CurSecs();
}

bool
CSrvSocketTask::Connect(Uint4 host, Uint2 port)
{
    if (m_Fd != -1) {
        s_CloseSocket(m_Fd, true);
        m_Fd = -1;
    }
    m_RegError = false;

#ifdef NCBI_OS_LINUX
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        LOG_WITH_ERRNO(Critical, "Cannot create socket", errno);
        goto error_return;
    }
    if (!s_SetSocketNonBlock(sock)  ||  !s_SetSocketOptions(sock))
        goto close_and_error;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = host;
    addr.sin_port = htons(port);
retry:
    int res;
    res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        int x_errno = errno;
        if (x_errno == EINTR)
            goto retry;
        if (x_errno != EINPROGRESS) {
            LOG_WITH_ERRNO(Critical, "Cannot connect socket", x_errno);
            goto close_and_error;
        }
    }

    m_ConnStartJfy = s_CurJiffies;
    m_Fd = sock;
    s_CreateDiagRequest(this, GetLocalPort(), host, port);
    GetCurThread()->stat->SockOpenActive();
    return true;

close_and_error:
    s_CloseSocket(sock, true);
error_return:
#endif
    return false;
}

bool
CSrvSocketTask::StartProcessing(TSrvThreadNum thread_num /* = 0 */)
{
    s_SaveSocket(this);
    m_LastThread = (thread_num? thread_num: GetCurThread()->thread_num);

#ifdef NCBI_OS_LINUX
    struct epoll_event evt;
    evt.events = EPOLLIN | EPOLLOUT | EPOLLET;
    evt.data.ptr = (SSrvSocketInfo*)this;
    int res = epoll_ctl(s_EpollFD, EPOLL_CTL_ADD, m_Fd, &evt);
    if (res) {
        LOG_WITH_ERRNO(Critical, "Cannot add socket to epoll", errno);
        return false;
    }
#endif

    SetRunnable();
    return true;
}

Uint2
CSrvSocketTask::GetLocalPort(void)
{
#ifdef NCBI_OS_LINUX
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, len);
    if (getsockname(m_Fd, (struct sockaddr*)&addr, &len) == 0)
        return ntohs(addr.sin_port);

    LOG_WITH_ERRNO(Critical, "Cannot read local port of socket", errno);
#endif

    return 0;
}

void
CSrvSocketTask::GetPeerAddress(string& host, Uint2& port)
{
    port = m_PeerPort;
    host = CTaskServer::IPToString(m_PeerAddr);
}

void
CSrvSocketTask::x_CloseSocket(bool do_abort)
{
    if (m_Fd == -1)
        return;
    s_CloseSocket(m_Fd, do_abort);
    s_CleanSockResources(this);
}

void
CSrvSocketTask::Terminate(void)
{
    if (m_Fd != -1)
        CloseSocket();
    MarkTaskTerminated(this, false);
}


CSrvSocketFactory::CSrvSocketFactory(void)
{}

CSrvSocketFactory::~CSrvSocketFactory(void)
{}

END_NCBI_SCOPE;
