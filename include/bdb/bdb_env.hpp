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

#include <bdb/bdb_types.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB
 *
 * @{
 */


/// BDB environment object a collection including support for some or 
/// all of caching, locking, logging and transaction subsystems.

class NCBI_BDB_EXPORT CBDB_Env
{
public:
    enum EEnvOptions {
        eThreaded = (1 << 0),          ///< corresponds to DB_THREAD 
        eRunRecovery = (1 << 1),       ///< Run DB recovery first
        eRunRecoveryFatal = (1 << 2)   ///< Run DB recovery first
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
    void Open(const char* db_home, int flags);

    /// Open environment with database locking (DB_INIT_LOCK)
    ///
    /// @param db_home destination directory for the database
    void OpenWithLocks(const char* db_home);

    /// Open environment with CDB locking (DB_INIT_CDB)
    ///
    /// @param db_home destination directory for the database
    void OpenConcurrentDB(const char* db_home);

    /// Open environment using transaction
    ///
    /// @param db_home 
    ///    destination directory for the database
    /// @param flags
    
    void OpenWithTrans(const char* db_home, TEnvOpenFlags opt = 0);

    /// Open error reporting file for the environment
    ///
    /// @param 
    ///    file_name - name of the error file
    ///    if file_name == "stderr" or "stdout" 
    ///    all errors are redirected to that device
    void OpenErrFile(const char* file_name);

    /// Join the existing environment
    ///
    /// @param 
    ///    db_home destination directory for the database
    /// @param 
    ///    opt environment options (see EEnvOptions)
    /// @sa EEnvOptions
    void JoinEnv(const char* db_home, TEnvOpenFlags opt = 0);

    /// Return underlying DB_ENV structure pointer for low level access.
    DB_ENV* GetEnv() { return m_Env; }

    /// Set cache size for the environment.
    void SetCacheSize(unsigned int cache_size);
    
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
    bool IsTransactional() const { return m_Transactional; }

    /// Flush the underlying memory pools, logs and data bases
    void TransactionCheckpoint();

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

    /// Remove all non-active log files
    void CleanLog();

    /// Set timeout value for locks in microseconds (1 000 000 in sec)
    void SetLockTimeout(unsigned timeout);

    /// Set timeout value for transactions in microseconds (1 000 000 in sec)
    void SetTransactionTimeout(unsigned timeout);

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
    
private:
    /// Opens BDB environment returns error code
    /// Throws no exceptions.
    int x_Open(const char* db_home, int flags);
    
private:
    CBDB_Env(const CBDB_Env&);
    CBDB_Env& operator=(const CBDB_Env&);
private:
    DB_ENV*  m_Env;
    bool     m_Transactional; ///< TRUE if environment is transactional
    FILE*    m_ErrFile;
    string   m_HomePath;
};

/* @} */

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2005/03/28 12:58:41  kuznets
 * + SetLogAutoRemove() SetLogBSize()
 *
 * Revision 1.20  2005/02/02 19:49:53  grichenk
 * Fixed more warnings
 *
 * Revision 1.19  2004/10/18 15:36:47  kuznets
 * +SetLogFileMax
 *
 * Revision 1.18  2004/08/24 14:06:13  kuznets
 * Added CBDB_ENv::CheckRemove()
 *
 * Revision 1.17  2004/08/13 15:56:19  kuznets
 * +eRunRecoveryFatal
 *
 * Revision 1.16  2004/08/13 11:02:53  kuznets
 * +Remove(), ForceRemove(), Close()
 *
 * Revision 1.15  2004/08/10 11:52:48  kuznets
 * +timeout control functions
 *
 * Revision 1.14  2004/08/09 16:27:35  kuznets
 * +CBDB_env::CleanLog()
 *
 * Revision 1.13  2004/06/21 15:04:44  kuznets
 * Added support of recovery open for environment
 *
 * Revision 1.12  2004/03/26 14:04:21  kuznets
 * + environment functions to fine tune buffering and force
 * transaction checkpoints
 *
 * Revision 1.11  2003/12/29 18:44:55  kuznets
 * JoinEnv() added parameter to specify environment options (thread safety, etc)
 *
 * Revision 1.10  2003/12/29 16:15:26  kuznets
 * +OpenErrFile
 *
 * Revision 1.9  2003/12/12 14:05:52  kuznets
 * + CBDB_Env::IsTransactional
 *
 * Revision 1.8  2003/12/10 19:13:07  kuznets
 * Added support of berkeley db transactions
 *
 * Revision 1.7  2003/11/24 13:49:08  kuznets
 * +OpenConcurrentDB
 *
 * Revision 1.6  2003/11/03 13:06:55  kuznets
 * + CBDB_Env::JoinEnv
 *
 * Revision 1.5  2003/10/20 15:23:38  kuznets
 * Added cache management for BDB environment
 *
 * Revision 1.4  2003/09/29 14:30:22  kuznets
 * Comments doxygenification
 *
 * Revision 1.3  2003/09/26 19:16:09  kuznets
 * Documentation changes
 *
 * Revision 1.2  2003/08/27 15:13:53  kuznets
 * Fixed DLL export spec
 *
 * Revision 1.1  2003/08/27 15:03:51  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* BDB_ENV__HPP */
