#ifndef NETCACHE__TASK_SERVER__HPP
#define NETCACHE__TASK_SERVER__HPP
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
 *   Declaration of the main class in TaskServer infrastructure and some
 *   utility classes that can be used with it.
 */


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE

/// Type for thread number in TaskServer
typedef Uint2 TSrvThreadNum;
/// Type for flags in CSrvTask. It's needed only for internal use, shouldn't
/// be necessary outside task_server library.
typedef Uint1 TSrvTaskFlags;


class CNcbiRegistry;
class CSrvSocketFactory;
class CSrvTime;


// Helper types to use in CSrvRCUUser
struct SSrvRCUList_tag;
typedef intr::slist_member_hook<intr::tag<SSrvRCUList_tag> >   TSrvRCUListHook;

/// Class to derive from to use RCU mechanism. For detailed explanation of
/// RCU go to http://lse.sourceforge.net/locking/rcupdate.html. In short:
/// if you want to do something which can race with other threads but you know
/// when other threads exit current functions to return to TaskServer it will
/// be already safe to do your job RCU is what you need. The most common use
/// is deletion of an object pointer to which is obtained from somewhere and
/// used locally in the same function and not remembered for later use.
class CSrvRCUUser
{
public:
    /// Method to be called to schedule call of ExecuteRCU() at appropriate
    /// time. CallRCU() cannot be called twice on the same object unless
    /// ExecuteRCU() was called in between those calls (it is safe to call
    /// CallRCU() from inside ExecutRCU() ).
    void CallRCU(void);
    /// Method implementing RCU job that was scheduled earlier by CallRCU().
    virtual void ExecuteRCU(void) = 0;

    CSrvRCUUser(void);
    virtual ~CSrvRCUUser(void);

private:
    CSrvRCUUser(const CSrvRCUUser&);
    CSrvRCUUser& operator= (const CSrvRCUUser&);

// Consider this section private as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    TSrvRCUListHook m_RCUListHook;
};

// Helper types to be used only inside TaskServer. It's placed here only to
// keep all relevant pieces in one place.
typedef intr::member_hook<CSrvRCUUser,
                          TSrvRCUListHook,
                          &CSrvRCUUser::m_RCUListHook>  TSrvRCUListOpt;
typedef intr::slist<CSrvRCUUser,
                    TSrvRCUListOpt,
                    intr::cache_last<true>,
                    intr::constant_time_size<true> >    TSrvRCUList;


// Helper types to be used in CSrvShutdownCallback.
struct SSrvShutdownList_tag;
typedef intr::slist_member_hook<intr::tag<SSrvShutdownList_tag> >   TSrvSDListHook;

/// Interface for an object wishing to be notified when server is going to
/// shutdown. Implementation can postpone shutdown for a while if it thinks
/// some operations should be finished first. To be notified object
/// implementing this interface should be registered with
/// CTaskServer::AddShutdownCallback().
class CSrvShutdownCallback
{
public:
    CSrvShutdownCallback(void);
    virtual ~CSrvShutdownCallback(void);

    /// Method called if server is ready to shutdown. Method should return
    /// TRUE if this object is ready for shutdown too, FALSE if shutdown
    /// should be delayed and this method should be called later.
    virtual bool ReadyForShutdown(void) = 0;

// Consider this section private as it's public for internal use only
// to minimize implementation-specific clutter in headers.
public:
    TSrvSDListHook m_SDListHook;
};

// Helper types to be used only inside TaskServer. It's placed here only to
// keep all relevant pieces in one place.
typedef intr::member_hook<CSrvShutdownCallback,
                          TSrvSDListHook,
                          &CSrvShutdownCallback::m_SDListHook>  TSDListHookOpt;
typedef intr::slist<CSrvShutdownCallback,
                    TSDListHookOpt,
                    intr::constant_time_size<false>,
                    intr::cache_last<false> >                   TShutdownList;


/// Types of server shutdown procedures that one can request from CTaskServer.
enum ESrvShutdownType {
    // "Slow" shutdown means TaskServer gives more time for current operations
    // to complete. Server stays in "soft" shutdown phase for up to
    // [task_server]/slow_shutdown_timeout seconds from ini-file.
    eSrvSlowShutdown,
    // "Fast" shutdown means TaskSerer should stop quicker. With this shutdown
    // type server will stay in "soft" shutdown phase for up to
    // [task_server]/fast_shutdown_timeout seconds from ini-file.
    eSrvFastShutdown
};


/// Main class representing TaskServer infrastructure. This is just a common
/// place for some global methods. There's no instance of this class anywhere.
class CTaskServer
{
public:
    /// Initializes TaskServer infrastructure from given command line
    /// arguments. Call to this method should be virtually the first call
    /// in the main() function. Returns TRUE if initialization was successful,
    /// FALSE otherwise. In case of unsuccessful initialization application
    /// should exit asap.
    static bool Initialize(int& argc, const char** argv);
    /// Finalizes TaskServer infrastructure. Method should be called even if
    /// Initialize() returned FALSE to give TaskServer a chance to finalize
    /// parts that were already initialized.
    static void Finalize(void);
    /// Run all TaskServer machinery. Method doesn't return until server was
    /// asked to stop via RequestShutdown() method or SIGINT/SIGTERM signals.
    /// After receiving those requests to stop method still doesn't return
    /// immediately - it waits for all operations to complete (server to
    /// become idle), all sockets to close and all shutdown callbacks to show
    /// readiness for shutdown.
    static void Run(void);

    /// Obtains reference to registry read from application's ini-file.
    static const CNcbiRegistry& GetConfRegistry(void);
    /// Returns time when this server application was started (when
    /// Initialize() method was called).
    static CSrvTime GetStartTime(void);

    /// Adds port for TaskServer to listen to. factory will create
    /// CSrvSocketTask objects for all sockets connecting to this port.
    /// TaskServer has a limit of 16 ports to listen to, attempts to call
    /// method to add more ports will result in FALSE returned. Also if this
    /// method is called while Run() is executing it can return FALSE if port
    /// is busy or some other network-related error occurred. In all other
    /// cases method returns TRUE (port is actually bound and opened
    /// for listening only inside Run().
    static bool AddListeningPort(Uint2 port, CSrvSocketFactory* factory);
    /// Adds new object wishing to receive callbacks when server is going to
    /// shutdown and wishing to influence this behavior.
    static void AddShutdownCallback(CSrvShutdownCallback* callback);
    /// Asks server to start shutdown procedures. These procedures include
    /// closing all listening sockets, notifying all sockets (so that they
    /// could start appropriate closing procedures) and waiting for everything
    /// to calm down, for all sockets to close and for all shutdown callbacks
    /// to signal their readiness.
    static void RequestShutdown(ESrvShutdownType shutdown_type);

    /// Checks if TaskServer is running now, i.e. Run() method is called and
    /// shutdown is not requested.
    static bool IsRunning(void);
    /// Checks if TaskServer received request to shutdown.
    static bool IsInShutdown(void);
    /// Checks if TaskServer currently is in "soft" shutdown phase when
    /// client-related operations still can proceed to finish.
    static bool IsInSoftShutdown(void);
    /// Checks if TaskServer currently is in "hard" shutdown phase when
    /// no operation is allowed to proceed unless there's no way to stop it.
    static bool IsInHardShutdown(void);

    /// Returns maximum number of worker threads allowed
    /// ([task_server]/max_threads parameter in ini-file).
    static TSrvThreadNum GetMaxRunningThreads(void);
    /// Returns number of current worker thread. Number is 0-based.
    static TSrvThreadNum GetCurThreadNum(void);

    /// Returns name of server this application is executing on.
    static const string& GetHostName(void);
    /// Converts server name (or IP address written as string) to encoded
    /// 4-byte IP address.
    static Uint4 GetIPByHost(const string& host);
    /// Converts 4-byte encoded IP address into its string representation.
    static string IPToString(Uint4 ip);
    /// Converts 4-byte encoded IP address into server name.
    static string GetHostByIP(Uint4 ip);

private:
    CTaskServer(const CTaskServer&);
};


END_NCBI_SCOPE

#endif /* NETCACHE__TASK_SERVER__HPP */
