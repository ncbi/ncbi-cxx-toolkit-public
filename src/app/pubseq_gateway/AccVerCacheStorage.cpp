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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <string>

#include "AccVerCacheStorage.hpp"
#include "AccVerCacheDB.hpp"


/** CAccVerCacheStorage */

constexpr const char* const CAccVerCacheStorage::DATA_DB;

CAccVerCacheStorage::CAccVerCacheStorage(const lmdb::env &  env) :
    m_Env(env),
    m_Dbi({0}),
    m_IsReadOnly(true)
{
    unsigned int    flags = 0;
    unsigned int    env_flags = 0;

    lmdb::env_get_flags(m_Env, &env_flags);
    m_IsReadOnly = env_flags & MDB_RDONLY;

    auto    wtxn = lmdb::txn::begin(env, nullptr, env_flags & MDB_RDONLY);
    m_Dbi = lmdb::dbi::open(wtxn, DATA_DB, flags);
    wtxn.commit();
}


void CAccVerCacheStorage::s_InitializeDb(const lmdb::env &  env)
{
    unsigned int    flags = MDB_CREATE;
    auto            wtxn = lmdb::txn::begin(env);

    lmdb::dbi::open(wtxn, DATA_DB, flags);
    wtxn.commit();
}


bool CAccVerCacheStorage::Update(const string &  key,
                                 const string &  data, bool  check_if_exists)
{
    if (m_IsReadOnly)
        EAccVerException::raise("Can not update: DB open in readonly mode");
    if (check_if_exists) {
        auto        rtxn = lmdb::txn::begin(m_Env, nullptr, MDB_RDONLY);
        string      ex_data;
        bool        found = m_Dbi.get(rtxn, key, ex_data);

        rtxn.commit();
        if (found && ex_data == data)
            return false;
    }
    auto    wtxn = lmdb::txn::begin(m_Env);
    m_Dbi.put(wtxn, key, data);
    wtxn.commit();
    return true;
}


bool CAccVerCacheStorage::Get(const string &  key, string &  data) const
{
    auto    rtxn = lmdb::txn::begin(m_Env, nullptr, MDB_RDONLY);
    bool    found = m_Dbi.get(rtxn, key, data);

    rtxn.commit();
    return found;
}
