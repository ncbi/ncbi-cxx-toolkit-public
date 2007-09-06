#ifndef BDB_ENV__HPP
#define BDB_ENV__HPP

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Wrapper around Berkeley DB environment
 *
 */

/// @file bdb_env.hpp
/// Wrapper around Berkeley DB environment structure

#include <corelib/ncbiobj.hpp>

#include <bdb/bdb_types.hpp>
#include <bdb/bdb_trans.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE

class CBDB_CheckPointThread;
struct IServer_Monitor;

/** @addtogroup BDB
 *
 * @{
 */

class CBDB_Transaction;

/// BDB environment object a collection including support for some or 
/// all of caching, locking, logging and transaction subsystems.

class NCBI_BDB_EXPORT CBDB_Env
{
public:
    enum EEnvOptions {
        eThreaded = (1 << 0),          ///< corresponds to DB_THREAD 
        eRunRecovery = (1 << 1),       ///< Run DB recovery first
        eRunRecoveryFatal = (1 << 2),  ///< Run DB recovery first
        ePrivate = (1 << 3)            ///< Create private directory
    };

    /// Environment types
    enum EEnvType {
        eEnvLocking,
        eEnvConcurrent,
        eEnvTransactional
    };
    
    /// OR-ed combination of EEnvOptions    
    typedef unsigned int  TEnvOpenFlags;
    
public:
    CBDB_Env();

    /// Construct CBDB_Env taking ownership of external DB_ENV. 
    ///
    /// Presumed that env has been opened with all required parameters.
    /// Class takes the ownership on the provided DB_ENV object.
    CBDB_Env(DB_ENV* env);

    ~CBDB_Env();

    /// Open environment
    ///
    /// @param db_home destination directory for the database
    /// @param flags - Berkeley DB flags (see documentation on DB_ENV->open)
    void Open(const string& db_home, int flags);

    /// Open environment with database locking (DB_INIT_LOCK)
    ///
    /// @param db_home destination directory for the database
    void OpenWithLocks(const string& db_home);

    /// Open-create private environment
    void OpenPrivate(const string& db_home);

    /// Open environment with CDB locking (DB_INIT_CDB)
    ///
    /// @param db_home destination directory for the database
    void OpenConcurrentDB(const string& db_home);

    /// Open environment using transaction
    ///
    /// @param db_home 
    ///    destination directory for the database
    /// @param flags
    
    void OpenWithTrans(const string& db_home, TEnvOpenFlags opt = 0);

    /// Open error reporting file for the environment
    ///
    /// @param 
    ///    file_name - name of the error file
    ///    if file_name == "stderr" or "stdout" 
    ///    all errors are redirected to that device
    void OpenErrFile(const string& file_name);

    /// Set the prefix used during reporting of errors
    void SetErrPrefix(const string& s);

    /// Modes to test if environment is transactional or not
    ///
    enum ETransactionDiscovery {
        eTestTransactions,    ///< Do a test to discover transactions
        eInspectTransactions, ///< Ask the joined environment if it supports
                              ///< transactions
        eAssumeTransactions,  ///< Assume env. is transactional
        eAssumeNoTransactions ///< Assume env. is non-transactional
    };

    /// Join the existing environment
    ///
    /// @param  db_home 
    ///    destination directory for the database
    /// @param  opt 
    ///    environment options (see EEnvOptions)
    /// @sa EEnvOptions
    void JoinEnv(const string& db_home, 
                 TEnvOpenFlags opt = 0,
                 ETransactionDiscovery trans_test = eTestTransactions);

    /// Return underlying DB_ENV structure pointer for low level access.
    DB_ENV* GetEnv() { return m_Env; }

    /// Set cache size for the environment.
    /// We also have the option of setting the total number of caches to use
    /// The default is 1
    void SetCacheSize(Uint8 cache_size,
                      int   num_caches = 1);
    
    /// Start transaction (DB_ENV->txn_begin)
    /// 
    /// @param parent_txn
    ///   Parent transaction
    /// @param flags
    ///   Transaction flags
    /// @return 
    ///   New transaction handler
    DB_TXN* CreateTxn(DB_TXN* parent_txn = 0, unsigned int flags = 0);

    /// Return TRUE if environment has been open as transactional
    bool IsTransactional() const;

    /// Flush the underlying memory pools, logs and data bases
    void TransactionCheckpoint();

    /// Forced checkpoint
    void ForceTransactionCheckpoint();

    /// Turn off buffering of databases (DB_DIRECT_DB)
    void SetDirectDB(bool on_off);

    /// Turn off buffering of log files (DB_DIRECT_LOG)
    void SetDirectLog(bool on_off);

    /// If set, Berkeley DB will automatically remove log files that are no 
    /// longer needed. (Can make catastrofic recovery impossible).
    void SetLogAutoRemove(bool on_off);

    /// Set maximum size of LOG files
    void SetLogFileMax(unsigned int lg_max);

    /// Set the size of the in-memory log buffer, in bytes.
    void SetLogBSize(unsigned lg_bsize);

    /// Configure environment for non-durable in-memory logging
    void SetLogInMemory(bool on_off);

    /// Path to directory where transaction logs are to be stored
    /// By default it is the same directory as environment
    void SetLogDir(const string& log_dir);

    bool IsLogInMemory() const { return m_LogInMemory; }

    /// Set max number of locks in the database
    ///
    /// see DB_ENV->set_lk_max_locks for more details
    void SetMaxLocks(unsigned locks);

    /// Get max locks
    unsigned GetMaxLocks();

    /// see DB_ENV->set_lk_max_objects for more details
    void SetMaxLockObjects(unsigned lock_obj_max);

    /// Set the maximum number of locking entities supported by the Berkeley DB environment.
    void SetMaxLockers(unsigned max_lockers);

    /// Remove all non-active log files
    void CleanLog();

    /// Set timeout value for locks in microseconds (1 000 000 in sec)
    void SetLockTimeout(unsigned timeout);

    /// Set timeout value for transactions in microseconds (1 000 000 in sec)
    void SetTransactionTimeout(unsigned timeout);

    /// Set number of active transaction
    /// see DB_ENV->set_tx_max for more details
    void SetTransactionMax(unsigned tx_max);

    /// Specify that test-and-set mutexes should spin tas_spins times 
    /// without blocking
    void SetTasSpins(unsigned tas_spins);

    /// Configure the total number of mutexes
    void MutexSetMax(unsigned max);

    /// Get number of mutexes
    unsigned MutexGetMax();

    /// Configure the number of additional mutexes to allocate.
    void MutexSetIncrement(unsigned inc);

    unsigned MutexGetIncrement();

    /// Get number of free mutexes
    unsigned MutexGetFree();

    /// Print mutex statistics
    void PrintMutexStat(CNcbiOstream & out);

    /// Non-force removal of BDB environment. (data files remains intact).
    /// @return
    ///   FALSE if environment is busy and cannot be deleted
    bool Remove();

    /// Force remove BDB environment.
    void ForceRemove();

    /// Try to Remove the environment, if DB_ENV::remove returns 0, but fails
    /// files ramain on disk anyway calls ForceRemove
    /// @return
    ///   FALSE if environment is busy and cannot be deleted
    bool CheckRemove();

    /// Close the environment;
    void Close();

    /// Reset log sequence number
    void LsnReset(const char* file_name);

    /// Reset log sequence number if environment logs are in memory
    void LsnResetForMemLog(const char* file_name);

    /// Get default syncronicity setting
    CBDB_Transaction::ETransSync GetTransactionSync() const
    {
        return m_TransSync;
    }

    /// Set default syncronicity level
    void SetTransactionSync(CBDB_Transaction::ETransSync sync);

    /// Print lock statistics
    void PrintLockStat(CNcbiOstream & out);

    /// Print memory statistics
    void PrintMemStat(CNcbiOstream & out);


    /// Ensures that a specified percent of the pages in 
    /// the shared memory pool are clean, by writing dirty pages to 
    /// their backing files
    ///
    /// @param percent 
    ///     percent of the pages in the cache that should be clean
    /// @param nwrotep
    ///     Number of pages written
    ///
    void MempTrickle(int percent, int *nwrotep);

    /// Flushes modified pages in the cache to their backing files
    void MempSync();

    /// limits the number of sequential write operations scheduled by the 
    /// library when flushing dirty pages from the cache
    ///
    /// @param maxwrite
    ///     maximum number of sequential writes to perform
    /// @param maxwrite_sleep
    ///     sleep time in microseconds between write attempts
    void SetMpMaxWrite(int maxwrite, int maxwrite_sleep);

    /// Set the maximal size for mmap
    void SetMpMmapSize(size_t map_size);

    /// Get the maximal size for mmap
    size_t GetMpMmapSize();

    /// return the path to the environment
    const string& GetPath() const { return m_HomePath; }

    /// If the kb parameter is non-zero, a checkpoint will be done 
    /// if more than kbyte kilobytes of log data have been written since 
    /// the last checkpoint.
    void SetCheckPointKB(unsigned kb) { m_CheckPointKB = kb; }

    /// If the m parameter is non-zero, a checkpoint will be done 
    /// if more than min minutes have passed since the last checkpoint.
    void SetCheckPointMin(unsigned m) { m_CheckPointMin = m; }

    /// When OFF calls to TransactionCheckpoint() will be ignored
    void EnableCheckPoint(bool on_off) { m_CheckPointEnable = on_off; }

    /// @name Deadlock detection
    /// @{


    /// Deadlock detection modes
    ///
    enum EDeadLockDetect {
        eDeadLock_Disable,   ///< No deadlock detection performed
        eDeadLock_Default,  ///< Default deadlock detector
        eDeadLock_MaxLocks, ///< Abort trans with the greatest number of locks
        eDeadLock_MinWrite, ///< Abort trans with the fewest write locks
        eDeadLock_Oldest,   ///< Abort the oldest trans
        eDeadLock_Random,   ///< Abort a random trans involved in the deadlock
        eDeadLock_Youngest  ///< Abort the youngest trans
    };

    /// Set deadlock detect mode (call before open).
    /// By default deadlock detection is disabled
    ///
    void SetLkDetect(EDeadLockDetect detect_mode);

    /// Run deadlock detector
    void DeadLockDetect();

    ///@}

    /// @name Background writer
    /// @{

    /// Background work flags
    ///
    enum EBackgroundWork {
        eBackground_MempTrickle     = (1 << 0),
        eBackground_Checkpoint      = (1 << 1),
        eBackground_DeadLockDetect  = (1 << 2),

    };

    /// Background work flags (combination of EBackgroundWork)
    ///
    typedef unsigned TBackgroundFlags;

    /// Schedule background maintenance thread which will do:
    ///    - transaction checkpointing
    ///    - background write (memory trickle)
    ///    - deadlock resolution
    ///
    /// (Call when environment is open)
    ///
    /// @param flags
    ///     Flag-mask setting what thread should do
    /// @param thread_delay
    ///     sleep in seconds between every call
    /// @param memp_trickle 
    ///     percent of dirty pages flushed to disk
    /// @param err_max
    ///     Max.tolerable number of errors (when exceeded - thread stops)
    ///     0 - unrestricted
    ///
    void RunBackgroundWriter(TBackgroundFlags flags, 
                             unsigned thread_delay = 30,
                             int memp_trickle = 0,
                             unsigned err_max = 0);

    /// Stop transaction checkpoint thread
    void StopBackgroundWriterThread();

    /// Deprecated version of RunBackgroundWriter
    void RunCheckpointThread(unsigned thread_delay = 30,
                             int memp_trickle = 0,
                             unsigned err_max = 0);

    /// Stop transaction checkpoint thread
    void StopCheckpointThread();

    ///@}

    /// @name Monitor
    /// @{

    /// Set monitor (class does NOT take ownership)
    ///
    /// Monitor should be set at right after construction,
    /// before using cache. (Especially important in MT environment)
    ///
    void SetMonitor(IServer_Monitor* monitor) { m_Monitor = monitor; }

    /// Get monitor
    IServer_Monitor* GetMonitor() { return m_Monitor; }

    ///@}

private:
    /// Opens BDB environment returns error code
    /// Throws no exceptions.
    int x_Open(const char* db_home, int flags);

    unsigned x_GetDeadLockDetect(EDeadLockDetect detect_mode);
    
private:
    /// forbidden
    CBDB_Env(const CBDB_Env&);
    CBDB_Env& operator=(const CBDB_Env&);
private:
    DB_ENV*                      m_Env;
    bool                         m_Transactional; ///< TRUE if transactional
    FILE*                        m_ErrFile;
    string                       m_ErrPrefix;
    string                       m_HomePath;
    bool                         m_LogInMemory;
    CBDB_Transaction::ETransSync m_TransSync;
    unsigned                     m_MaxLocks;
    unsigned                     m_MaxLockers;
    unsigned                     m_MaxLockObjects;
    bool                         m_DirectDB;
    bool                         m_DirectLOG;
    bool                         m_CheckPointEnable; ///< Checkpoint enabled
    unsigned                     m_CheckPointKB;     ///< Checkpoint KBytes
    unsigned                     m_CheckPointMin;    ///< Checkpoint minutes
    EDeadLockDetect              m_DeadLockMode;

    CRef<CBDB_CheckPointThread>  m_CheckThread;   ///< Checkpoint thread    
    IServer_Monitor*             m_Monitor;       ///< Monitoring interface
};


/// Run Berkeley DB recovery in private environment.
/// Derived from db_recover.
///
NCBI_BDB_EXPORT
void BDB_RecoverEnv(const string& path,
                    bool          fatal_recover);


/* @} */

END_NCBI_SCOPE

#endif  /* BDB_ENV__HPP */
