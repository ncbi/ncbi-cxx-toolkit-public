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
: m_Env(0)
{
    int ret = db_env_create(&m_Env, 0);
    BDB_CHECK(ret, "DB_ENV");
}

CBDB_Env::CBDB_Env(DB_ENV* env)
: m_Env(env)
{
}

CBDB_Env::~CBDB_Env()
{
    /*int ret = */m_Env->close(m_Env, 0);
}

void CBDB_Env::Open(const char* db_home, int flags)
{
    int ret = m_Env->open(m_Env, db_home, flags, 0664);
    BDB_CHECK(ret, "DB_ENV");
}


void CBDB_Env::OpenWithLocks(const char* db_home)
{
    Open(db_home, DB_INIT_LOCK);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/08/27 15:04:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
