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

#include <bdb/bdb_env.hpp>
#include <db.h>

BEGIN_NCBI_SCOPE

CBDB_Env::CBDB_Env()
: m_Env(0),
  m_Transactional(false)
{
    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV");
}

CBDB_Env::CBDB_Env(DB_ENV* env)
: m_Env(env),
  m_Transactional(false)
{
}

CBDB_Env::~CBDB_Env()
{
    /*int ret = */m_Env->close(m_Env, 0);
}

void CBDB_Env::SetCacheSize(unsigned int cache_size)
{
    int ret = m_Env->set_cachesize(m_Env, 0, cache_size, 1);
    BDB_CHECK(ret, 0);
}


void CBDB_Env::Open(const char* db_home, int flags)
{
    int ret = m_Env->open(m_Env, db_home, flags, 0664);
    BDB_CHECK(ret, "DB_ENV");
}


void CBDB_Env::OpenWithLocks(const char* db_home)
{
    Open(db_home, DB_CREATE|DB_RECOVER|DB_INIT_LOCK|DB_INIT_MPOOL);
}

void CBDB_Env::OpenWithTrans(const char* db_home)
{
    int ret = m_Env->set_lk_detect(m_Env, DB_LOCK_DEFAULT);
    BDB_CHECK(ret, "DB_ENV");

    Open(db_home, 
         DB_INIT_TXN | DB_RECOVER |
         DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL | DB_THREAD);
    m_Transactional = true;
}

void CBDB_Env::OpenConcurrentDB(const char* db_home)
{
    int ret = 
      m_Env->set_flags(m_Env, 
                       DB_CDB_ALLDB|DB_DIRECT_DB, 1);
    BDB_CHECK(ret, "DB_ENV::set_flags");

    Open(db_home, DB_INIT_CDB|DB_INIT_MPOOL);
}

void CBDB_Env::JoinEnv(const char* db_home)
{
    Open(db_home, DB_JOINENV);

    // Check if we joined the transactional environment

    DB_TXN_STAT *txn_statp;
    int ret = m_Env->txn_stat(m_Env, &txn_statp, 0);
    if (ret == 0)
    {
        if (m_Env->flags & DB_ENV_THREAD) {
            m_Transactional = true;
        }
        ::free(txn_statp);
    }
}

DB_TXN* CBDB_Env::CreateTxn(DB_TXN* parent_txn, unsigned int flags)
{
    DB_TXN* txn = 0;
    int ret = m_Env->txn_begin(m_Env, parent_txn, &txn, flags);
    BDB_CHECK(ret, "DB_ENV::txn_begin");
    return txn;
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2003/12/29 12:55:35  kuznets
 * Minor tweaking of locking mechanism.
 *
 * Revision 1.8  2003/12/15 14:51:43  kuznets
 * Checking environment flags in JoinEnv to determine environment is
 * transactional.
 *
 * Revision 1.7  2003/12/12 14:07:40  kuznets
 * JoinEnv: checking if we joining transactional environment
 *
 * Revision 1.6  2003/12/10 19:14:02  kuznets
 * Added support of berkeley db transactions
 *
 * Revision 1.5  2003/11/24 13:49:19  kuznets
 * +OpenConcurrentDB
 *
 * Revision 1.4  2003/11/03 13:07:22  kuznets
 * JoinEnv implemented
 *
 * Revision 1.3  2003/10/20 15:23:55  kuznets
 * Added cache management for BDB environment
 *
 * Revision 1.2  2003/08/27 20:04:29  kuznets
 * Set the correct combination of open flags (CBDB_Env::OpenWithLocks)
 *
 * Revision 1.1  2003/08/27 15:04:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
