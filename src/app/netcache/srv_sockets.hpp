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


/// For TaskServer's internal use only.
/// This structure is used as base for both CSrvSocketTask and internal
/// listening socket structure. Pointer to SSrvSocketInfo is remembered in
/// epoll and value of is_listening is used to distinguish between listening
/// socket structure and CSrvSocketTask.
struct SSrvSocketInfo
{
    bool is_listening;
};


struct SSrvSockList_tag;
typedef intr::list_member_hook<intr::tag<SSrvSockList_tag> >    TSrvSockListHook;

/// Task controlling a socket.
/// This task always becomes runnable in 3 cases:
/// - when socket changes from non-readable to readable (some data came into
///   socket and can be read by user);
/// - when socket changes from non-writable to writable (socket has been just
///   opened, connection established and data can be written to the other side);
/// - when some error appears in socket, e.g. client disconnected.
/// When task became runnable and ExecuteSlice() is called it's user's code
/// responsibility to process event completely (not necessarily immediately
/// but eventually). I.e. TaskServer won't set the task runnable again if user
/// didn't read all the data from socket.
/// CSrvSocketTask cannot attach to any random socket. It should be either
/// returned from CSrvSocketFactory and TaskServer will attach it to just
/// connected socket, or CSrvSocketTask::Connect() method should be called
/// to create socket with other server.
class CSrvSocketTask : public virtual CSrvTask, public SSrvSocketInfo
{
public:
    CSrvSocketTask(void);
    virtual ~CSrvSocketTask(void);

    /// Checks if there's some data available for reading in internal buffers.
    /// Method doesn't say anything about data available in socket but not yet
    /// retrieved from kernel.
    bool IsReadDataAvailable(void);
    /// Checks if there's some data pending in write buffers and waiting to be
    /// sent to kernel.
    bool IsWriteDataPending(void);
    /// Checks if socket has some error in it.
    bool HasError(void);
    /// Checks if socket can ever have more data to read even though it may not
    /// have it at the moment. Socket won't have more data to read if EOF
    /// received.
    bool CanHaveMoreRead(void);
    /// Checks if socket should be closed because of long inactivity or
    /// because server is in "hard" shutdown phase. User code should always
    /// check result of this method at some pivot points where it has the
    /// ability to close.
    bool NeedToClose(void);
    /// Checks if socket should be closed because of internal reasons (long
    /// inactivity or "hard" shutdown as in NeedToClose()) or because of
    /// external reasons, like error or EOF in the socket.
    bool NeedEarlyClose(void);

    /// Read from socket into internal buffer.
    /// Method tries to completely fill the buffer even if there's some data
    /// already waiting in it.
    bool ReadToBuf(void);
    /// Read from socket into memory.
    /// Method combines reading from internal buffer and directly from socket
    /// (without changing data order) and reads as much as possible to do
    /// without blocking. Method returns total size of read data (can be 0 if
    /// no data immediately available).
    size_t Read(void* buf, size_t size);
    /// Read from socket one line which ends with '\n', '\r\n' or '\0'.
    /// Line read should fit into internal read buffer, i.e. it shouldn't be
    /// too long, otherwise method returns FALSE and raises error flag which
    /// means you won't be able to anything else with socket (besides closing).
    /// Method returns TRUE if line was successfully read and FALSE if there's
    /// no data or no line end markers in it. When method returned TRUE
    /// variable line will point to internal read buffer. Thus it should be
    /// used before calling any other Read*() methods -- after that memory can
    /// become invalid.
    bool ReadLine(CTempString* line);
    /// Read from socket exactly the given data size. If given size is not
    /// immediately available (without blocking) method doesn't read anything
    /// and returns FALSE. Otherwise data is copied to buf and TRUE is
    /// returned. Requested size must be less than internal read buffer.
    bool ReadData(void* buf, Uint2 size);
    /// Read from socket a number in native machine representation.
    /// If sizeof(NumType) bytes is not immediately available in the socket
    /// method returns FALSE and doesn't read anything. Otherwise it copies
    /// data into num and returns TRUE.
    template <typename NumType>
    bool ReadNumber(NumType* num);

    /// Write text into socket.
    /// The whole text message will be written, internal write buffer will be
    /// expanded to accommodate the message if necessary.
    CSrvSocketTask& WriteText(CTempString message);
    /// Write number into socket as string, i.e. number is converted to string
    /// using NStr::NumericToString() first and then the result is written to
    /// socket as string. The whole number will be written, internal write
    /// buffer will be expanded to accommodate it if necessary.
    template <typename NumType>
    CSrvSocketTask& WriteNumber(NumType num);
    /// Write the exact amount of data into the socket. All the data will be
    /// written, internal write buffer will be expanded to accommodate it
    /// if necessary.
    void WriteData(const void* buf, size_t size);
    /// Write into the socket as much as immediately possible (including
    /// writing into internal write buffers and writing directly into socket)
    /// without blocking and without expanding write buffer. Method returns
    /// amount of data written which can be 0 if socket is not writable at the
    /// moment.
    size_t Write(const void* buf, size_t size);
    /// Flush all data saved in internal write buffers to socket.
    /// Method must be called from inside of ExecuteSlice() of this task and
    /// no other writing methods should be called until FlushIsDone() returns
    /// TRUE.
    void Flush(void);
    /// Request flushing of all data saved in internal write buffers to socket.
    /// Method can be called from anywhere even concurrently with
    /// ExecuteSlice() of this task. No other writing methods should be called
    /// until FlushIsDone() returns TRUE.
    void RequestFlush(void);
    /// Check if data flushing requested earlier is complete.
    bool FlushIsDone(void);

    /// Start proxying of raw data from this socket to the one in dst_task.
    /// Method must be called from inside of ExecuteSlice() of this task. And
    /// nothing else should be done with both sockets until IsProxyInProgress()
    /// returns FALSE. Only proxy_size bytes is transferred between sockets.
    /// Proxying can stop earlier only if NeedEarlyClose() on either socket
    /// returns true. In this case ProxyHadError() will return true and
    /// nothing else could be done with sockets except closing.
    void StartProxyTo(CSrvSocketTask* dst_task, Uint8 proxy_size);
    /// Check whether proxying started earlier is still in progress.
    bool IsProxyInProgress(void);
    /// Check whether proxying started earlier finished successfully or any of
    /// sockets had some error in it.
    bool ProxyHadError(void);

    /// Create new socket and connect it to given IP and port.
    /// If operation is successful TRUE is returned, FALSE returned otherwise.
    /// FALSE will be returned only in case of errors like network interface
    /// is down or things like that. If server is inaccessible then Connect()
    /// will return TRUE but later there will be error in the socket.
    bool Connect(Uint4 host, Uint2 port);
    /// Start processing of the socket and include it into TaskServer's
    /// central epoll. Until this method is called TaskServer won't make this
    /// task runnable and won't check if it's readable or writable -- any
    /// Read/Write operation will be unsuccessful (processing 0 bytes).
    bool StartProcessing(TSrvThreadNum thread_num = 0);
    /// Close the socket gracefully, i.e. if any socket's data is still in
    /// kernel's buffers and didn't make it to wire it will be sent later. But
    /// internal task's write buffers won't be flushed.
    void CloseSocket(void);
    /// Abort the socket, i.e. anything unsent in kernel buffers will be
    /// dropped.
    void AbortSocket(void);

    /// Get peer IP and port for this socket.
    void GetPeerAddress(string& host, Uint2& port);
    /// Get local port this socket was created on.
    Uint2 GetLocalPort(void);

    /// Terminate the task. Method is overridden here to add some extra
    /// processing for sockets.
    virtual void Terminate(void);

private:
    CSrvSocketTask(const CSrvSocketTask&);
    CSrvSocketTask& operator= (const CSrvSocketTask&);

    /// Internal function to execute time slice work. Overridden here to add
    /// some extra socket processing before and after call to user's code in
    /// ExecuteSlice().
    virtual void InternalRunSlice(TSrvThreadNum thr_num);

    /// Close or abort the socket -- they have little difference, thus they
    /// joined in one method.
    void x_CloseSocket(bool do_abort);
    /// Prints socket's error if there's any error pending on the socket.
    void x_PrintError(void);

// Consider this section private as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    /// Source task for proxying.
    /// This variable is set to non-NULL value only in destination tasks for
    /// proxying.
    CSrvSocketTask* m_ProxySrc;
    /// Destination task for proxying.
    /// This variable is set to non-NULL value only in source tasks for
    /// proxying.
    CSrvSocketTask* m_ProxyDst;
    /// Amount left to proxy if proxying operation is in progress.
    /// Value is set only in source tasks for proxying.
    Uint8 m_ProxySize;
    /// Hook to include the task to lists of open sockets.
    TSrvSockListHook m_SockListHook;
    /// Read buffer.
    char* m_RdBuf;
    /// Write buffer.
    char* m_WrBuf;
    /// Jiffy number when Connect() method was called. Variable is used to
    /// track connection timeouts.
    Uint4 m_ConnStartJfy;
    /// Time (as time_t) when the socket was active last time, i.e. when
    /// InternalRunSlice() was called last time.
    int m_LastActive;
    /// File descriptor for the socket.
    int m_Fd;
    /// Size of data available for reading in the read buffer.
    Uint2 m_RdSize;
    /// Position of current reading in the read buffer, i.e. all data in
    /// [0, m_RdPos) was already read; data in [m_RdPos, m_RdSize) are queued
    /// for reading.
    Uint2 m_RdPos;
    /// Size of memory allocated for write buffer. This size can grow in
    /// methods requiring exact writing like WriteData(), WriteText() etc.
    Uint2 m_WrMemSize;
    /// Size of data in the write buffer waiting for writing.
    Uint2 m_WrSize;
    /// Position of current writing pointer in the write buffer.
    Uint2 m_WrPos;
    /// Flag showing if '\r' symbol was seen at the end of last line but '\n'
    /// wasn't seen yet.
    bool m_CRMet;
    /// Flag showing that last proxying operation finished with error.
    bool m_ProxyHadError;
    /// Flag showing that socket is readable.
    bool m_SockHasRead;
    /// Flag showing that socket is writable.
    bool m_SockCanWrite;
    /// Flag showing that socket can have more reads, i.e. there was no EOF yet.
    bool m_SockCanReadMore;
    /// Flag showing that socket needs to be closed because of long
    /// inactivity.
    bool m_NeedToClose;
    /// Flag showing that task needs to flush all write buffers.
    bool m_NeedToFlush;
    /// Flag showing that write buffers were flushed.
    bool m_FlushIsDone;
    /// Number of last read event seen by Read() when it read from socket. If
    /// this number is equal to m_RegReadEvts then no new "readable" events
    /// came from epoll and thus result of last read defines if there is more
    /// data in the socket to read (if last read returned less data than
    /// provided buffer size then there's no more data in the socket).
    Uint1 m_SeenReadEvts;
    /// Number of last write event seen by Write() when it wrote to socket. If
    /// this number is equal to m_RegWriteEvts then no new "writable" events
    /// came from epoll and thus result of last write defines if more data
    /// can be written in the socket (if last write have written less data than
    /// provided buffer size then nothing else can be written in the socket).
    Uint1 m_SeenWriteEvts;
    /// Total number of bytes read from socket.
    Uint8 m_ReadBytes;
    /// Total number of bytes written to socket.
    Uint8 m_WrittenBytes;
    /// Counter of "readable" events received from epoll.
    Uint1 m_RegReadEvts;
    /// Counter of "writable" events received from epoll.
    Uint1 m_RegWriteEvts;
    /// Flag showing if epoll returned RDHUP on this socket.
    bool m_RegReadHup;
    /// Flag showing if there's error pending on the socket.
    bool m_RegError;
    /// Flag showing if pending error in socket was printed in logs.
    bool m_ErrorPrinted;
    /// Remembered peer port. Value valid only for sockets created from
    /// listening sockets.
    Uint2 m_PeerPort;
    /// Remembered peer IP address. Value valid only for sockets created from
    /// listening sockets.
    Uint4 m_PeerAddr;
    ///
    string m_ConnReqId;
};


// Helper types to be used only inside TaskServer. It's placed here only to
// keep all relevant pieces in one place.
typedef intr::member_hook<CSrvSocketTask,
                          TSrvSockListHook,
                          &CSrvSocketTask::m_SockListHook>  TSockListHookOpt;
typedef intr::list<CSrvSocketTask,
                   TSockListHookOpt,
                   intr::constant_time_size<false> >        TSockList;


/// Factory that creates CSrvSocketTask-derived object for each connection
/// coming to listening port which this factory is assigned to.
class CSrvSocketFactory
{
public:
    CSrvSocketFactory(void);
    virtual ~CSrvSocketFactory(void);

    virtual CSrvSocketTask* CreateSocketTask(void) = 0;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_SOCKETS__HPP */
