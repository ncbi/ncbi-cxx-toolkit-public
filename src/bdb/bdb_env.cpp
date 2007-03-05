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
#include <bdb/bdb_env.hpp>
#include <db.h>


// Berkeley DB 4.4.x renamed "set_tas_spins" to "mutex_set_tas_spins"
#if DB_VERSION_MAJOR >= 4 
    #if DB_VERSION_MINOR >= 4
        #define BDB_USE_MUTEX_TAS
    #endif
#endif



BEGIN_NCBI_SCOPE

CBDB_Env::CBDB_Env()
: m_Env(0),
  m_Transactional(false),
  m_ErrFile(0),
  m_LogInMemory(false)
{
    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV");
}

CBDB_Env::CBDB_Env(DB_ENV* env)
: m_Env(env),
  m_Transactional(false),
  m_ErrFile(0),
  m_LogInMemory(false)
{
}

CBDB_Env::~CBDB_Env()
{
    try {
        Close();
    } 
    catch (CBDB_Exception&)
    {}
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

void CBDB_Env::SetCacheSize(unsigned int cache_size)
{
    int ret = m_Env->set_cachesize(m_Env, 0, cache_size, 1);
    BDB_CHECK(ret, 0);
}

void CBDB_Env::SetLogBSize(unsigned lg_bsize)
{
    int ret = m_Env->set_lg_bsize(m_Env, lg_bsize);
    BDB_CHECK(ret, 0);
}


void CBDB_Env::Open(const string& db_home, int flags)
{
    int ret = x_Open(db_home.c_str(), flags);
    BDB_CHECK(ret, "DB_ENV");
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
            LOG_POST(Warning << "BDB_Env: Trying fatal recovery.");
            if ((ret == DB_RUNRECOVERY) && (flags & DB_INIT_TXN)) {
                recover_flag &= ~DB_RECOVER;
                recover_flag = flags | DB_RECOVER_FATAL | DB_CREATE;

                ret = m_Env->open(m_Env, db_home, recover_flag, 0664);
                if (ret) {
                    LOG_POST(Warning << 
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
    Open(db_home, DB_CREATE/*|DB_RECOVER*/|DB_INIT_LOCK|DB_INIT_MPOOL);
}

void CBDB_Env::OpenPrivate(const string& db_home)
{
    int ret = x_Open(db_home.c_str(), 
                     DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL);
    BDB_CHECK(ret, "DB_ENV");
}


void CBDB_Env::OpenWithTrans(const string& db_home, TEnvOpenFlags opt)
{
    int ret = m_Env->set_lk_detect(m_Env, DB_LOCK_DEFAULT);
    BDB_CHECK(ret, "DB_ENV");
    
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
        BDB_CHECK(ret, "DB_ENV");
        
        // non-private recovery
        if (!(recover_flag & DB_PRIVATE)) {
            m_Transactional = true;
            return;
        }

        // "Private" recovery requires reopening, to make
        // it available for other programs

        ret = m_Env->close(m_Env, 0);
        BDB_CHECK(ret, "DB_ENV");
        
        ret = db_env_create(&m_Env, 0);
        BDB_CHECK(ret, "DB_ENV");
    } else 
    if (opt & eRunRecoveryFatal) {
        // use private environment as prescribed by "db_recover" utility
        int recover_flag = flag | DB_RECOVER_FATAL | DB_CREATE; 
        
        if (!(recover_flag & DB_SYSTEM_MEM)) {
            recover_flag |= DB_PRIVATE;
        }
        
        ret = x_Open(db_home.c_str(), recover_flag);
        BDB_CHECK(ret, "DB_ENV");
        
        // non-private recovery
        if (!(recover_flag & DB_PRIVATE)) {
            m_Transactional = true;
            return;
        }

        // "Private" recovery requires reopening, to make
        // it available for other programs

        ret = m_Env->close(m_Env, 0);
        BDB_CHECK(ret, "DB_ENV");
        
        ret = db_env_create(&m_Env, 0);
        BDB_CHECK(ret, "DB_ENV");
    }

    

    Open(db_home,  flag);
    m_Transactional = true;
}

void CBDB_Env::OpenConcurrentDB(const string& db_home)
{
    int ret = 
      m_Env->set_flags(m_Env, DB_CDB_ALLDB | DB_DIRECT_DB, 1);
    BDB_CHECK(ret, "DB_ENV::set_flags");

    Open(db_home, DB_INIT_CDB | DB_INIT_MPOOL);
}

void CBDB_Env::JoinEnv(const string& db_home, TEnvOpenFlags opt)
{
    int flag = DB_JOINENV;
    if (opt & eThreaded) {
        flag |= DB_THREAD;
    }
    
    Open(db_home, flag);
    
    // Check if we joined the transactional environment
    // Try to create a fake transaction to test the environment
    DB_TXN* txn = 0;
    int ret = m_Env->txn_begin(m_Env, 0, &txn, 0);

    if (ret == 0) {
        m_Transactional = true;
        ret = txn->abort(txn);
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
    _ASSERT(!m_HomePath.empty());
    Close();

    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV");

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
    BDB_CHECK(ret, "DB_ENV");

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
    _ASSERT(locks);
    int ret = m_Env->set_lk_max_locks(m_Env, locks);
    BDB_CHECK(ret, "DB_ENV::set_lk_max_locks");
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

unsigned CBDB_Env::GetMaxLocks()
{
    u_int32_t lk_max;
    int ret = m_Env->get_lk_max_locks(m_Env, &lk_max);
    BDB_CHECK(ret, "DB_ENV::get_lk_max_locks");
    return lk_max;
}

void CBDB_Env::SetMaxLockObjects(unsigned lock_obj_max)
{
    _ASSERT(lock_obj_max);
    int ret = m_Env->set_lk_max_objects(m_Env, lock_obj_max);
    BDB_CHECK(ret, "DB_ENV::set_lk_max_objects");
}

void CBDB_Env::SetLogInMemory(bool on_off)
{
    int ret = m_Env->set_flags(m_Env, DB_LOG_INMEMORY, on_off);
    BDB_CHECK(ret, "DB_ENV::set_flags");
    m_LogInMemory = on_off;
}

void CBDB_Env::SetTasSpins(unsigned tas_spins)
{
#ifdef BDB_USE_MUTEX_TAS
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
    // error checking commented (not all platforms support direct IO)
    /*int ret = */ m_Env->set_flags(m_Env, DB_DIRECT_DB, (int)on_off);
    // BDB_CHECK(ret, "DB_ENV::set_flags(DB_DIRECT_DB)");   
}

void CBDB_Env::SetDirectLog(bool on_off)
{
    // error checking commented (not all platforms support direct IO)
    /*int ret = */ m_Env->set_flags(m_Env, DB_DIRECT_LOG, (int)on_off);
    // BDB_CHECK(ret, "DB_ENV::set_flags(DB_DIRECT_LOG)");   
}

void CBDB_Env::SetLogAutoRemove(bool on_off)
{
    int ret = m_Env->set_flags(m_Env, DB_LOG_AUTOREMOVE, (int)on_off);
    BDB_CHECK(ret, "DB_ENV::set_flags(DB_LOG_AUTOREMOVE)");
}


void CBDB_Env::TransactionCheckpoint()
{
    if (IsTransactional()) {
        int ret = m_Env->txn_checkpoint(m_Env, 0, 0, 0);
        BDB_CHECK(ret, "DB_ENV::txn_checkpoint");   
    }
}

bool CBDB_Env::IsTransactional() const 
{ 
    return m_Transactional; 
}

void CBDB_Env::CleanLog()
{
    char **nm_list = 0;
	int ret = 
        m_Env->log_archive(m_Env, &nm_list, DB_ARCH_ABS);
    BDB_CHECK(ret, "DB_ENV::CleanLog()");

	if (nm_list != NULL) {
        for (char** file = nm_list; *file != NULL; ++file) {
            LOG_POST(Info << "BDB_Env: Removing LOG file: " << *file);
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


END_NCBI_SCOPE
