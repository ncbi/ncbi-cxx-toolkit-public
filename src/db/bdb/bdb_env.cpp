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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary environment class implementation.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>

#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_checkpoint_thread.hpp>

#include <db/error_codes.hpp>

#include <connect/server_monitor.hpp>

#include <db.h>


// Berkeley DB sometimes changes the inteface so some calls have to be adjusted
// depending on the version
#ifdef BDB_FULL_VERSION
    #undef BDB_FULL_VERSION
#endif
#define BDB_FULL_VERSION    DB_VERSION_PATCH + \
                            1000 * DB_VERSION_MINOR + \
                            1000 * 1000 * DB_VERSION_MAJOR



// Berkeley DB 4.4.x reworked and extended the mutex API.
#if DB_VERSION_MAJOR >= 4
    #if DB_VERSION_MINOR >= 4 || DB_VERSION_MAJOR > 4
        #define BDB_NEW_MUTEX_API
    #endif
    #if DB_VERSION_MINOR >= 5 || DB_VERSION_MAJOR > 4
        #define BDB_NEW_MEM_STATS
    #endif
#endif


#define NCBI_USE_ERRCODE_X   Db_Bdb_Env


BEGIN_NCBI_SCOPE

CBDB_Env::CBDB_Env()
    : m_Env(0),
      m_Transactional(false),
      m_ErrFile(0),
      m_LogInMemory(false),
      m_TransSync(CBDB_Transaction::eTransSync),
      m_MaxLocks(0),
      m_MaxLockers(0),
      m_MaxLockObjects(0),
      m_DirectDB(false),
      m_DirectLOG(false),
      m_CheckPointEnable(true),
      m_CheckPointKB(0),
      m_CheckPointMin(0),
      m_DeadLockMode(eDeadLock_Disable),
      m_Monitor(0)
{
    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV::create");
}


CBDB_Env::CBDB_Env(DB_ENV* env)
    : m_Env(env),
      m_Transactional(false),
      m_ErrFile(0),
      m_LogInMemory(false),
      m_TransSync(CBDB_Transaction::eTransSync),
      m_MaxLocks(0),
      m_MaxLockers(0),
      m_MaxLockObjects(0),
      m_DirectDB(false),
      m_DirectLOG(false),
      m_CheckPointEnable(true),
      m_CheckPointKB(0),
      m_CheckPointMin(0),
      m_DeadLockMode(eDeadLock_Disable),
      m_Monitor(0)
{
}

CBDB_Env::~CBDB_Env()
{
    try {
        Close();
    }
    catch (CBDB_Exception&) {
    }
    if (m_ErrFile) {
        fclose(m_ErrFile);
    }
}

void CBDB_Env::Close()
{
    if (m_Env) {
        int ret = m_Env->close(m_Env, 0);
        m_Env = 0;
        BDB_CHECK(ret, "DB_ENV::close");
    }
}

void CBDB_Env::SetTransactionSync(CBDB_Transaction::ETransSync sync)
{
    if (sync == CBDB_Transaction::eEnvDefault)
        m_TransSync = CBDB_Transaction::eTransSync;
    else
        m_TransSync = sync;

    if (sync == CBDB_Transaction::eTransSync) {
        int  ret = m_Env->set_flags(m_Env, DB_DSYNC_DB, 1);
        BDB_CHECK(ret, "DB_ENV::set_flags(DB_DSYNC_DB)");
    }
}


void CBDB_Env::SetCacheSize(Uint8 cache_size,
                            int   num_caches)
{
    unsigned cache_g = (unsigned) (cache_size / (Uint8)1073741824);  // gig
    if (cache_g) {
        cache_size = cache_size % 1073741824;
    }
    unsigned ncache = max(num_caches, 1);
    int ret =
        m_Env->set_cachesize(m_Env, cache_g, (unsigned)cache_size, ncache);
    BDB_CHECK(ret, "DB_ENV::set_cachesize");
}

void CBDB_Env::SetLogRegionMax(unsigned size)
{
    int ret = m_Env->set_lg_regionmax(m_Env, size);
    BDB_CHECK(ret, "DB_ENV::set_lg_regionmax");
}

void CBDB_Env::SetLogBSize(unsigned lg_bsize)
{
    int ret = m_Env->set_lg_bsize(m_Env, lg_bsize);
    BDB_CHECK(ret, "DB_ENV::set_lg_bsize");
}

void CBDB_Env::Open(const string& db_home, int flags)
{
    int ret = x_Open(db_home.c_str(), flags);
    BDB_CHECK(ret, "DB_ENV::open");

    SetDirectDB(m_DirectDB);
    SetDirectLog(m_DirectLOG);
}

int CBDB_Env::x_Open(const char* db_home, int flags)
{
    int recover_requested = flags & DB_RECOVER;

    int ret = m_Env->open(m_Env, db_home, flags, 0664);
    if (ret == DB_RUNRECOVERY) {
        if (flags & DB_JOINENV) {  // join do not attempt recovery
            return ret;
        }
        int recover_flag;

        if (!recover_requested) {
            if (flags & DB_INIT_TXN) { // recovery needs transaction
                recover_flag = flags | DB_RECOVER | DB_CREATE;
            } else {
                goto fatal_recovery;
            }
        } else {
            goto fatal_recovery;
        }

        ret = m_Env->open(m_Env, db_home, recover_flag, 0664);

        if (!recover_requested) {
            fatal_recovery:
            LOG_POST_X(1, Warning << "BDB_Env: Trying fatal recovery.");
            if ((ret == DB_RUNRECOVERY) && (flags & DB_INIT_TXN)) {
                recover_flag &= ~DB_RECOVER;
                recover_flag = flags | DB_RECOVER_FATAL | DB_CREATE;

                ret = m_Env->open(m_Env, db_home, recover_flag, 0664);
                if (ret) {
                    LOG_POST_X(2, Warning <<
                               "Fatal recovery returned error code=" << ret);
                }
            }
        }
    }
    m_HomePath = db_home;
    return ret;
}

void CBDB_Env::OpenWithLocks(const string& db_home)
{
    Open(db_home, DB_CREATE/*|DB_RECOVER*/|DB_THREAD|DB_INIT_LOCK|DB_INIT_MPOOL);
}

void CBDB_Env::OpenPrivate(const string& db_home)
{
    int ret = x_Open(db_home.c_str(),
                     DB_CREATE|DB_THREAD|DB_PRIVATE|DB_INIT_MPOOL);
    BDB_CHECK(ret, "DB_ENV::open_private");
}


void CBDB_Env::OpenWithTrans(const string& db_home, TEnvOpenFlags opt)
{
    int ret = m_Env->set_lk_detect(m_Env, DB_LOCK_DEFAULT);
    BDB_CHECK(ret, "DB_ENV::set_lk_detect");

    if (m_MaxLockObjects) {
        this->SetMaxLockObjects(m_MaxLockObjects);
    }
    if (m_MaxLocks) {
        this->SetMaxLocks(m_MaxLocks);
    }
    if (m_MaxLockers) {
        this->SetMaxLockers(m_MaxLockers);
    }

    int flag =  DB_INIT_TXN |
                DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL;

    // for windows we try to init environment in system memory
    // (not file based)
    // so it is reclaimed by OS automatically even if the application crashes
# ifdef NCBI_OS_MSWIN
    flag |= DB_SYSTEM_MEM;
# endif
    if (opt & eThreaded) {
        flag |= DB_THREAD;
    }
    if (opt & ePrivate) {
        flag |= DB_PRIVATE;
    }

    // Run recovery procedure, reinitialize the environment

    if (opt & eRunRecovery) {
        // use private environment as prescribed by "db_recover" utility
        int recover_flag = flag | DB_RECOVER | DB_CREATE;

        if (!(recover_flag & DB_SYSTEM_MEM)) {
            recover_flag |= DB_PRIVATE;
        }

        ret = x_Open(db_home.c_str(), recover_flag);
        BDB_CHECK(ret, "DB_ENV::open");

        // non-private recovery
        if (!(recover_flag & DB_PRIVATE)) {
            m_Transactional = true;
            return;
        }

        // "Private" recovery requires reopening, to make
        // it available for other programs

        ret = m_Env->close(m_Env, 0);
        BDB_CHECK(ret, "DB_ENV::close");

        ret = db_env_create(&m_Env, 0);
        BDB_CHECK(ret, "DB_ENV::create");
    } else
    if (opt & eRunRecoveryFatal) {
        // use private environment as prescribed by "db_recover" utility
        int recover_flag = flag | DB_RECOVER_FATAL | DB_CREATE;

        if (!(recover_flag & DB_SYSTEM_MEM)) {
            recover_flag |= DB_PRIVATE;
        }

        ret = x_Open(db_home.c_str(), recover_flag);
        BDB_CHECK(ret, "DB_ENV::open");

        // non-private recovery
        if (!(recover_flag & DB_PRIVATE)) {
            m_Transactional = true;
            return;
        }

        // "Private" recovery requires reopening, to make
        // it available for other programs

        ret = m_Env->close(m_Env, 0);
        BDB_CHECK(ret, "DB_ENV::close");

        ret = db_env_create(&m_Env, 0);
        BDB_CHECK(ret, "DB_ENV::create");
    }

    Open(db_home,  flag);
    m_Transactional = true;
}

void CBDB_Env::OpenConcurrentDB(const string& db_home)
{
    int ret = m_Env->set_flags(m_Env, DB_CDB_ALLDB, 1);
    BDB_CHECK(ret, "DB_ENV::set_flags(DB_CDB_ALLDB)");

    Open(db_home, DB_CREATE | DB_THREAD | DB_INIT_CDB | DB_INIT_MPOOL);
}

void CBDB_Env::JoinEnv(const string& db_home,
                       TEnvOpenFlags opt,
                       ETransactionDiscovery trans_test)
{
    int flag = DB_JOINENV;
    if (opt & eThreaded) {
        flag |= DB_THREAD;
    }

    Open(db_home, flag);


    switch (trans_test) {
    case eTestTransactions:
        {{
             // Check if we joined the transactional environment
             // Try to create a fake transaction to test the environment
             DB_TXN* txn = 0;
             int ret = m_Env->txn_begin(m_Env, 0, &txn, 0);

             if (ret == 0) {
                 m_Transactional = true;
                 ret = txn->abort(txn);
             }
         }}
        break;

    case eInspectTransactions:
        {{
             // Check if we joined the transactional environment
             Uint4 flags = 0;
             int ret = m_Env->get_open_flags(m_Env, &flags);
             BDB_CHECK(ret, "DB_ENV::get_open_flags");
             if (flags & DB_INIT_TXN) {
                 m_Transactional = true;
             } else {
                 m_Transactional = false;
             }
         }}
        break;
    case eAssumeTransactions:
        m_Transactional = true;
        break;
    case eAssumeNoTransactions:
        m_Transactional = false;
        break;
    default:
        _ASSERT(0);
    }

    // commented since it caused crash on windows trying to free
    // txn_statp structure. (Why it happened remains unknown)

/*
    DB_TXN_STAT *txn_statp = 0;
    int ret = m_Env->txn_stat(m_Env, &txn_statp, 0);
    if (ret == 0)
    {
        ::free(txn_statp);
        txn_statp = 0;

        // Try to create a fake transaction to test the environment
        DB_TXN* txn = 0;
        ret = m_Env->txn_begin(m_Env, 0, &txn, 0);

        if (ret == 0) {
            m_Transactional = true;
            ret = txn->abort(txn);
        }
    }
*/
}

bool CBDB_Env::Remove()
{
    if (m_HomePath.empty()) {
        return true;
    }
    Close();

    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV::create");

    ret = m_Env->remove(m_Env, m_HomePath.c_str(), 0);
    m_Env = 0;

    if (ret == EBUSY)
        return false;

    BDB_CHECK(ret, "DB_ENV::remove");
    return true;
}

void CBDB_Env::ForceRemove()
{
    _ASSERT(!m_HomePath.empty());
    Close();

    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV::create");

    ret = m_Env->remove(m_Env, m_HomePath.c_str(), DB_FORCE);
    m_Env = 0;
    BDB_CHECK(ret, "DB_ENV::remove");
}

bool CBDB_Env::CheckRemove()
{
    if (Remove()) {
        // Remove returned OK, but BerkeleyDB has a bug(?)
        // that it does not even try to remove environment
        // when it cannot attach to it.
        // In case of Windows and opening was made using DB_SYSTEM_MEM
        // it does not remove files.

        // so we check that and force the removal
        CDir dir(m_HomePath);
        CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);
        if (!fl.empty()) {
            ForceRemove();
        }
        return true;
    }
    return false;
}

void CBDB_Env::SetErrPrefix(const string& prefix)
{
    _ASSERT(m_Env);
    m_ErrPrefix = prefix;
    m_Env->set_errpfx(m_Env, m_ErrPrefix.c_str());
}

void CBDB_Env::SetLogDir(const string& log_dir)
{
    if (log_dir.empty()) {
        return;
    }
    try {
        CDir dir(log_dir);
        if (!dir.Exists()) {
            if (!dir.Create()) {
                ERR_POST_X(3, "Cannot create transaction log directory:" << log_dir);
                return;
            }
        }
        int ret = m_Env->set_lg_dir(m_Env, log_dir.c_str());
        BDB_CHECK(ret, "DB_ENV::set_lg_dir");
    }
    catch(exception& ex)
    {
        ERR_POST_X(4, "Cannot set transaction log directory:" << ex.what());
    }
}

DB_TXN* CBDB_Env::CreateTxn(DB_TXN* parent_txn, unsigned int flags)
{
    DB_TXN* txn = 0;
    int ret = m_Env->txn_begin(m_Env, parent_txn, &txn, flags);
    BDB_CHECK(ret, "DB_ENV::txn_begin");
    return txn;
}

void CBDB_Env::SetLogFileMax(unsigned int lg_max)
{
    int ret = m_Env->set_lg_max(m_Env, lg_max);
    BDB_CHECK(ret, "DB_ENV::set_lg_max");
}

void CBDB_Env::SetMaxLocks(unsigned locks)
{
    m_MaxLocks = locks;
    if (m_Env) {
        int ret = m_Env->set_lk_max_locks(m_Env, locks);
        BDB_CHECK(ret, "DB_ENV::set_lk_max_locks");
    }
}


void CBDB_Env::SetTransactionMax(unsigned tx_max)
{
    _ASSERT(tx_max);
    int ret = m_Env->set_tx_max(m_Env, tx_max);
    BDB_CHECK(ret, "DB_ENV::set_tx_max");
}


void CBDB_Env::LsnReset(const char* file_name)
{
    int ret = m_Env->lsn_reset(m_Env, const_cast<char*>(file_name), 0);
    BDB_CHECK(ret, "DB_ENV::lsn_reset");
}

void CBDB_Env::LsnResetForMemLog(const char* file_name)
{
    if (!m_LogInMemory) return;
    int ret = m_Env->lsn_reset(m_Env, const_cast<char*>(file_name), 0);
    BDB_CHECK(ret, "DB_ENV::lsn_reset");
}

unsigned CBDB_Env::GetMaxLocks()
{
    if (!m_Env)
        return m_MaxLocks;

    u_int32_t lk_max;
    int ret = m_Env->get_lk_max_locks(m_Env, &lk_max);
    BDB_CHECK(ret, "DB_ENV::get_lk_max_locks");
    return lk_max;
}

void CBDB_Env::SetMaxLockObjects(unsigned lock_obj_max)
{
    m_MaxLockObjects = lock_obj_max;
    if (m_Env) {
        int ret = m_Env->set_lk_max_objects(m_Env, m_MaxLockObjects);
        BDB_CHECK(ret, "DB_ENV::set_lk_max_objects");
    }
}

void CBDB_Env::SetMaxLockers(unsigned max_lockers)
{
    m_MaxLockers = max_lockers;
    if (m_Env) {
        int ret = m_Env->set_lk_max_lockers(m_Env, max_lockers);
        BDB_CHECK(ret, "DB_ENV::set_lk_max_lockers");
    }
}


void CBDB_Env::SetLogInMemory(bool on_off)
{
    #if BDB_FULL_VERSION < 4007000
        int ret = m_Env->set_flags(m_Env, DB_LOG_INMEMORY, int(on_off));
        BDB_CHECK(ret, "DB_ENV::set_flags(DB_LOG_INMEMORY)");
    #else
        int ret = m_Env->log_set_config(m_Env, DB_LOG_IN_MEMORY, (int)on_off);
        BDB_CHECK(ret, "DB_ENV::log_set_config(DB_LOG_IN_MEMORY)");
    #endif
    m_LogInMemory = on_off;
}

void CBDB_Env::SetTasSpins(unsigned tas_spins)
{
    #ifdef BDB_NEW_MUTEX_API
        int ret = m_Env->mutex_set_tas_spins(m_Env, tas_spins);
        BDB_CHECK(ret, "DB_ENV::mutex_set_tas_spins");
    #else
        int ret = m_Env->set_tas_spins(m_Env, tas_spins);
        BDB_CHECK(ret, "DB_ENV::set_tas_spins");
    #endif
}

void CBDB_Env::OpenErrFile(const string& file_name)
{
    if (m_ErrFile) {
        fclose(m_ErrFile);
        m_ErrFile = 0;
    }

    if (file_name == "stderr") {
        m_Env->set_errfile(m_Env, stderr);
        return;
    }
    if (file_name == "stdout") {
        m_Env->set_errfile(m_Env, stdout);
        return;
    }

    m_ErrFile = fopen(file_name.c_str(), "a");
    if (m_ErrFile) {
        m_Env->set_errfile(m_Env, m_ErrFile);
    }
}

void CBDB_Env::SetDirectDB(bool on_off)
{
    m_DirectDB = on_off;
    if (m_Env) {
        // error checking commented (not all platforms support direct IO)
        /*int ret = */ m_Env->set_flags(m_Env, DB_DIRECT_DB, (int)on_off);
        // BDB_CHECK(ret, "DB_ENV::set_flags(DB_DIRECT_DB)");
    }
}

void CBDB_Env::SetDirectLog(bool on_off)
{
    m_DirectLOG = on_off;
    if (m_Env) {
        // error checking commented (not all platforms support direct IO)
        #if BDB_FULL_VERSION < 4007000
        /*int ret = */ m_Env->set_flags(m_Env, DB_DIRECT_LOG, (int)on_off);
        // BDB_CHECK(ret, "DB_ENV::set_flags(DB_DIRECT_LOG)");
        #else
        /*int ret = */ m_Env->log_set_config(m_Env, DB_LOG_DIRECT, (int)on_off);
        // BDB_CHECK(ret, "DB_ENV::log_set_config(DB_LOG_DIRECT)");
        #endif
    }
}

void CBDB_Env::SetLogAutoRemove(bool on_off)
{
    #if BDB_FULL_VERSION < 4007000
        int ret = m_Env->set_flags(m_Env, DB_LOG_AUTOREMOVE, (int)on_off);
        BDB_CHECK(ret, "DB_ENV::set_flags(DB_LOG_AUTOREMOVE)");
    #else
        int ret = m_Env->log_set_config(m_Env, DB_LOG_AUTO_REMOVE, (int)on_off);
        BDB_CHECK(ret, "DB_ENV::log_set_config(DB_LOG_AUTO_REMOVE)");
    #endif
}


void CBDB_Env::TransactionCheckpoint()
{
    if (m_CheckPointEnable && IsTransactional()) {
        int ret =
            m_Env->txn_checkpoint(m_Env, m_CheckPointKB, m_CheckPointMin, 0);
        BDB_CHECK(ret, "DB_ENV::txn_checkpoint");
    }
}

void CBDB_Env::ForceTransactionCheckpoint()
{
    if (IsTransactional()) {
        int ret =
            m_Env->txn_checkpoint(m_Env, 0, 0, DB_FORCE);
        BDB_CHECK(ret, "DB_ENV::txn_checkpoint");
    }
}


bool CBDB_Env::IsTransactional() const
{
    return m_Transactional;
}


void CBDB_Env::LogFlush()
{
    BDB_CHECK(m_Env->log_flush(m_Env, 0), "DB_ENV::log_flush");
}


void CBDB_Env::CleanLog()
{
    char **nm_list = 0;
    int ret =
        m_Env->log_archive(m_Env, &nm_list, DB_ARCH_ABS);
    BDB_CHECK(ret, "DB_ENV::CleanLog()");

    if (nm_list != NULL) {
        for (char** file = nm_list; *file != NULL; ++file) {
            LOG_POST_X(5, Info << "BDB_Env: Removing LOG file: " << *file);
            CDirEntry de(*file);
            de.Remove();
        }
        free(nm_list);
    }

}

void CBDB_Env::SetLockTimeout(unsigned timeout)
{
    db_timeout_t to = timeout;
    int ret = m_Env->set_timeout(m_Env, to, DB_SET_LOCK_TIMEOUT);
    BDB_CHECK(ret, "DB_ENV::set_timeout");
}

void CBDB_Env::SetTransactionTimeout(unsigned timeout)
{
    db_timeout_t to = timeout;
    int ret = m_Env->set_timeout(m_Env, to, DB_SET_TXN_TIMEOUT);
    BDB_CHECK(ret, "DB_ENV::set_timeout");
}

void CBDB_Env::MutexSetMax(unsigned max)
{
#ifdef BDB_NEW_MUTEX_API
    int ret = m_Env->mutex_set_max(m_Env, max);
    BDB_CHECK(ret, "DB_ENV::mutex_set_max");
#endif
}

unsigned CBDB_Env::MutexGetMax()
{
#ifdef BDB_NEW_MUTEX_API
    u_int32_t maxp;
    int ret = m_Env->mutex_get_max(m_Env, &maxp);
    BDB_CHECK(ret, "DB_ENV::mutex_get_max");
    return maxp;
#else
    return 0;
#endif
}

void CBDB_Env::MutexSetIncrement(unsigned inc)
{
#ifdef BDB_NEW_MUTEX_API
    int ret = m_Env->mutex_set_increment(m_Env, inc);
    BDB_CHECK(ret, "DB_ENV::mutex_set_increment");
#endif
}

unsigned CBDB_Env::MutexGetIncrement()
{
#ifdef BDB_NEW_MUTEX_API
    u_int32_t inc;
    int ret = m_Env->mutex_get_increment(m_Env, &inc);
    BDB_CHECK(ret, "DB_ENV::mutex_get_increment");
    return inc;
#else
    return 0;
#endif
}

unsigned CBDB_Env::MutexGetFree()
{
    unsigned free_m = 0;
#ifdef BDB_NEW_MUTEX_API
    DB_MUTEX_STAT* stp = 0;
    try {
        int ret = m_Env->mutex_stat(m_Env, &stp, 0);
        BDB_CHECK(ret, "DB_ENV::mutex_stat");
        free_m = stp->st_mutex_free;
    }
    catch (...)
    {
        if (stp) {
            ::free(stp);
        }
        throw;
    }

    if (stp) {
        ::free(stp);
    }
#endif
    return free_m;
}


void CBDB_Env::PrintMutexStat(CNcbiOstream & out)
{
#ifdef BDB_NEW_MUTEX_API
    DB_MUTEX_STAT* stp = 0;
    try {
        int ret = m_Env->mutex_stat(m_Env, &stp, 0);
        BDB_CHECK(ret, "DB_ENV::mutex_stat");

        out << "st_mutex_align     : " << stp->st_mutex_align     << NcbiEndl
            << "st_mutex_tas_spins : " << stp->st_mutex_tas_spins << NcbiEndl
            << "st_mutex_free      : " << stp->st_mutex_free      << NcbiEndl
            << "st_mutex_inuse     : " << stp->st_mutex_inuse     << NcbiEndl
            << "st_mutex_inuse_max : " << stp->st_mutex_inuse_max << NcbiEndl
            << "st_regsize         : " << stp->st_regsize         << NcbiEndl
            << "st_region_wait     : " << stp->st_region_wait     << NcbiEndl
            << "st_region_nowait   : " << stp->st_region_nowait   << NcbiEndl
        ;
    }
    catch (...)
    {
        if (stp) {
            ::free(stp);
        }
        throw;
    }

    if (stp) {
        ::free(stp);
    }
#endif
}

void CBDB_Env::PrintLockStat(CNcbiOstream & out)
{
#ifdef BDB_NEW_MUTEX_API
    DB_LOCK_STAT *stp = 0;
    try {

        int ret = m_Env->lock_stat(m_Env, &stp, 0);
        BDB_CHECK(ret, "DB_ENV::lock_stat");

        out << "st_id           : " << stp->st_id           << NcbiEndl
            << "st_cur_maxid    : " << stp->st_cur_maxid    << NcbiEndl
            << "st_nmodes       : " << stp->st_nmodes       << NcbiEndl
            << "st_maxlocks     : " << stp->st_maxlocks     << NcbiEndl
            << "st_maxlockers   : " << stp->st_maxlockers   << NcbiEndl
            << "st_maxobjects   : " << stp->st_maxobjects   << NcbiEndl
            << "st_nlocks       : " << stp->st_nlocks       << NcbiEndl
            << "st_maxnlocks    : " << stp->st_maxnlocks    << NcbiEndl
            << "st_nlockers     : " << stp->st_nlockers     << NcbiEndl
            << "st_maxnlockers  : " << stp->st_maxnlockers  << NcbiEndl
            << "st_nobjects     : " << stp->st_nobjects     << NcbiEndl
            << "st_maxnobjects  : " << stp->st_maxnobjects  << NcbiEndl
            << "st_nrequests    : " << stp->st_nrequests    << NcbiEndl
            << "st_nreleases    : " << stp->st_nreleases    << NcbiEndl
            << "st_nupgrade     : " << stp->st_nupgrade     << NcbiEndl
            << "st_ndowngrade   : " << stp->st_ndowngrade   << NcbiEndl
            << "st_lock_wait    : " << stp->st_lock_wait    << NcbiEndl
            << "st_lock_nowait  : " << stp->st_lock_nowait  << NcbiEndl
            << "st_ndeadlocks   : " << stp->st_ndeadlocks   << NcbiEndl
            << "st_locktimeout  : " << stp->st_locktimeout  << NcbiEndl
            << "st_nlocktimeouts: " << stp->st_nlocktimeouts << NcbiEndl
            << "st_txntimeout   : " << stp->st_txntimeout    << NcbiEndl
            << "st_ntxntimeouts : " << stp->st_ntxntimeouts  << NcbiEndl
            << "st_regsize      : " << stp->st_regsize       << NcbiEndl
            << "st_region_wait  : " << stp->st_region_wait   << NcbiEndl
            << "st_region_nowait: " << stp->st_region_nowait << NcbiEndl
        ;
    }
    catch (...)
    {
        if (stp) {
            ::free(stp);
        }
        throw;
    }

    if (stp) {
        ::free(stp);
    }

#endif
}

void CBDB_Env::PrintMemStat(CNcbiOstream & out)
{
    DB_MPOOL_STAT *stp = 0;
    try {

        int ret = m_Env->memp_stat(m_Env, &stp, 0, 0);
        BDB_CHECK(ret, "DB_ENV::memp_stat");


        out << "st_gbytes           : " << stp->st_gbytes          << NcbiEndl
            << "st_bytes            : " << stp->st_bytes           << NcbiEndl
            << "st_ncache           : " << stp->st_ncache          << NcbiEndl
            << "st_regsize          : " << stp->st_regsize         << NcbiEndl
            << "st_mmapsize         : " << stp->st_mmapsize        << NcbiEndl
            << "st_maxopenfd        : " << stp->st_maxopenfd       << NcbiEndl
            << "st_maxwrite         : " << stp->st_maxwrite        << NcbiEndl
            << "st_maxwrite_sleep   : " << stp->st_maxwrite_sleep  << NcbiEndl
            << "st_map              : " << stp->st_map             << NcbiEndl
            << "st_cache_hit        : " << stp->st_cache_hit       << NcbiEndl
            << "st_cache_miss       : " << stp->st_cache_miss      << NcbiEndl
            << "st_page_create      : " << stp->st_page_create     << NcbiEndl
            << "st_page_in          : " << stp->st_page_in         << NcbiEndl
            << "st_page_out         : " << stp->st_page_out        << NcbiEndl
            << "st_ro_evict         : " << stp->st_ro_evict        << NcbiEndl
            << "st_rw_evict         : " << stp->st_rw_evict        << NcbiEndl
            << "st_page_trickle     : " << stp->st_page_trickle    << NcbiEndl
            << "st_pages            : " << stp->st_pages           << NcbiEndl
            << "st_page_clean       : " << stp->st_page_clean      << NcbiEndl
            << "st_page_dirty       : " << stp->st_page_dirty      << NcbiEndl
            << "st_hash_buckets     : " << stp->st_hash_buckets    << NcbiEndl
            << "st_hash_searches    : " << stp->st_hash_searches   << NcbiEndl
            << "st_hash_longest     : " << stp->st_hash_longest    << NcbiEndl
            << "st_hash_examined    : " << stp->st_hash_examined   << NcbiEndl
            << "st_hash_nowait      : " << stp->st_hash_nowait     << NcbiEndl
            << "st_hash_wait        : " << stp->st_hash_wait       << NcbiEndl
#ifdef BDB_NEW_MEM_STATS
            << "st_hash_max_nowait  : " << stp->st_hash_max_nowait << NcbiEndl
#endif
            << "st_hash_max_wait    : " << stp->st_hash_max_wait   << NcbiEndl
            << "st_region_wait      : " << stp->st_region_wait     << NcbiEndl
            << "st_region_nowait    : " << stp->st_region_nowait   << NcbiEndl
#ifdef BDB_NEW_MEM_STATS
            << "st_mvcc_frozen      : " << stp->st_mvcc_frozen     << NcbiEndl
            << "st_mvcc_thawed      : " << stp->st_mvcc_thawed     << NcbiEndl
            << "st_mvcc_freed       : " << stp->st_mvcc_freed      << NcbiEndl
#endif
            << "st_alloc            : " << stp->st_alloc           << NcbiEndl
            << "st_alloc_buckets    : " << stp->st_alloc_buckets   << NcbiEndl
            << "st_alloc_max_buckets: " << stp->st_alloc_max_buckets << NcbiEndl
            << "st_alloc_pages      : " << stp->st_alloc_pages     << NcbiEndl
            << "st_alloc_max_pages  : " << stp->st_alloc_max_pages << NcbiEndl
#ifdef BDB_NEW_MEM_STATS
            << "st_io_wait          : " << stp->st_io_wait         << NcbiEndl
#endif
        ;

        int max_write;
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 6)  ||  DB_VERSION_MAJOR > 4
        db_timeout_t max_write_sleep;
#else
        int max_write_sleep;
#endif
        ret =
            m_Env->get_mp_max_write(m_Env, &max_write, &max_write_sleep);
        BDB_CHECK(ret, "DB_ENV::get_mp_max_write");

        out << "max_write (pages)          : " << max_write       << NcbiEndl
            << "max_write_sleep (microsec) : " << max_write_sleep << NcbiEndl
        ;

    }
    catch (...)
    {
        if (stp) {
            ::free(stp);
        }
        throw;
    }

    if (stp) {
        ::free(stp);
    }
}


void CBDB_Env::MempTrickle(int percent, int *nwrotep)
{
    int nwr;
    int ret = m_Env->memp_trickle(m_Env, percent, &nwr);
    BDB_CHECK(ret, "DB_ENV::memp_trickle");
    if (nwrotep) {
        *nwrotep = nwr;
    }
    if (m_Monitor && m_Monitor->IsActive()) {
        string msg = "BDB_ENV: memp_tricle ";
        msg += NStr::IntToString(percent);
        msg += "% written ";
        msg += NStr::IntToString(nwr);
        msg += " pages.";
        m_Monitor->Send(msg);
    }
}

void CBDB_Env::MempSync()
{
    int ret = m_Env->memp_sync(m_Env, 0);
    BDB_CHECK(ret, "DB_ENV::memp_sync");
}


void CBDB_Env::SetMpMaxWrite(int maxwrite, int maxwrite_sleep)
{
    int ret = m_Env->set_mp_max_write(m_Env, maxwrite, maxwrite_sleep);
    BDB_CHECK(ret, "DB_ENV::set_mp_max_write");
}


size_t CBDB_Env::GetMpMmapSize()
{
    size_t map_size = 0;
    int ret = m_Env->get_mp_mmapsize(m_Env, &map_size);
    BDB_CHECK(ret, "DB_ENV::get_mp_mmapsize");
    return map_size;
}


void CBDB_Env::SetMpMmapSize(size_t map_size)
{
    int ret = m_Env->set_mp_mmapsize(m_Env, map_size);
    BDB_CHECK(ret, "DB_ENV::set_mp_mmapsize");
}

unsigned CBDB_Env::x_GetDeadLockDetect(EDeadLockDetect detect_mode)
{
    u_int32_t detect = 0;
    switch (detect_mode)
    {
    case eDeadLock_Disable:
        return 0;
    case eDeadLock_Default:
        detect = DB_LOCK_DEFAULT;
        break;
    case eDeadLock_MaxLocks:
        detect = DB_LOCK_MAXLOCKS;
        break;
    case eDeadLock_MinWrite:
        detect = DB_LOCK_MINWRITE;
        break;
    case eDeadLock_Oldest:
        detect = DB_LOCK_OLDEST;
        break;
    case eDeadLock_Random:
        detect = DB_LOCK_RANDOM;
        break;
    case eDeadLock_Youngest:
        detect = DB_LOCK_YOUNGEST;
    default:
        _ASSERT(0);
    }
    return detect;
}


void CBDB_Env::SetLkDetect(EDeadLockDetect detect_mode)
{
    m_DeadLockMode = detect_mode;
    if (m_DeadLockMode == eDeadLock_Disable) {
        return;
    }
    u_int32_t detect = x_GetDeadLockDetect(detect_mode);

    int ret = m_Env->set_lk_detect(m_Env, detect);
    BDB_CHECK(ret, "DB_ENV::set_lk_detect");
}

void CBDB_Env::DeadLockDetect()
{
    if (m_DeadLockMode == eDeadLock_Disable) {
        return;
    }
    u_int32_t detect = x_GetDeadLockDetect(m_DeadLockMode);
    int aborted;
    int ret = m_Env->lock_detect(m_Env, 0, detect, &aborted);
    BDB_CHECK(ret, "lock_detect");
}

void CBDB_Env::RunBackgroundWriter(TBackgroundFlags flags,
                                   unsigned thread_delay,
                                   int memp_trickle,
                                   unsigned err_max)
{
# ifdef NCBI_THREADS
    LOG_POST_X(6, Info << "Starting BDB transaction checkpoint thread.");
    m_CheckThread.Reset(
        new CBDB_CheckPointThread(*this, memp_trickle, thread_delay, 5));
    m_CheckThread->SetMaxErrors(err_max);
    m_CheckThread->SetWorkFlag(flags);
    m_CheckThread->Run();
# else
    LOG_POST_X(7, Warning <<
     "Cannot run BDB transaction checkpoint thread in non-MT configuration.");
# endif
}


void CBDB_Env::RunCheckpointThread(unsigned thread_delay,
                                   int      memp_trickle,
                                   unsigned err_max)
{
    TBackgroundFlags flags = CBDB_Env::eBackground_MempTrickle |
                             CBDB_Env::eBackground_Checkpoint  |
                             CBDB_Env::eBackground_DeadLockDetect;

    RunBackgroundWriter(flags, thread_delay, memp_trickle, err_max);
}

void CBDB_Env::StopBackgroundWriterThread()
{
# ifdef NCBI_THREADS
    if (!m_CheckThread.Empty()) {
        LOG_POST_X(8, Info << "Stopping BDB transaction checkpoint thread...");
        m_CheckThread->RequestStop();
        m_CheckThread->Join();
        LOG_POST_X(9, Info << "BDB transaction checkpoint thread stopped.");
    }
# endif
}

void CBDB_Env::StopCheckpointThread()
{
    StopBackgroundWriterThread();
}

void BDB_RecoverEnv(const string& path,
                    bool          fatal_recover)
{
    DB_ENV  *dbenv;
    int      ret;
    if ((ret = db_env_create(&dbenv, 0)) != 0) {
        string msg =
            "Cannot create environment " + string(db_strerror(ret));
        BDB_THROW(eInvalidOperation, msg);
    }

    dbenv->set_errfile(dbenv, stderr);
    //if (verbose)
    //  (void)dbenv->set_verbose(dbenv, DB_VERB_RECOVERY, 1);

    u_int32_t flags = 0;
    flags |= DB_CREATE | DB_INIT_LOG |
           DB_INIT_MPOOL | DB_INIT_TXN | DB_USE_ENVIRON;
    flags |= fatal_recover ? DB_RECOVER_FATAL : DB_RECOVER;
    flags |= DB_PRIVATE;

    if ((ret = dbenv->open(dbenv, path.c_str(), flags, 0)) != 0) {
        dbenv->close(dbenv, 0);
        string msg =
            "Cannot open environment " + string(db_strerror(ret));
        BDB_THROW(eInvalidOperation, msg);
    }
    ret = dbenv->close(dbenv, 0);
}




END_NCBI_SCOPE
