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

#include <sys/stat.h>
#include <sstream>

#include "AccVerCacheDB.hpp"
#include <objtools/pubseq_gateway/impl/rpc/UtilException.hpp>

using namespace std;

#define DBI_COUNT       6
#define THREAD_COUNT    1024

#define MAP_SIZE_INIT   (256L * 1024 * 1024 * 1024)
#define MAP_SIZE_DELTA  (16L * 1024 * 1024 * 1024)

constexpr const char* const CAccVerCacheDB::META_DB;

/** CAccVerCacheDB */

void CAccVerCacheDB::s_InitializeDb(const lmdb::env &  env)
{
    unsigned int        flags;

    lmdb::env_get_flags(env, &flags);
    if (flags & MDB_RDONLY)
        return;

    auto    wtxn = lmdb::txn::begin(env);
    auto    meta_dbi = lmdb::dbi::open(wtxn, CAccVerCacheDB::META_DB,
                                       MDB_CREATE);
    wtxn.commit();

    CAccVerCacheStorage::s_InitializeDb(env);
}


void CAccVerCacheDB::Open(const string &  db_path,
                          bool  initialize, bool  readonly)
{
    m_DbPath = db_path;
    if (m_DbPath.empty())
        EAccVerException::raise("DB path is not specified");
    if (initialize && readonly)
        EAccVerException::raise("DB create & readonly flags are mutually exclusive");

    int             stat_rv;
    bool            need_sync = !initialize;
    struct stat     st;

    stat_rv = stat(m_DbPath.c_str(), &st);
    if (stat_rv != 0 || st.st_size == 0) {
        /* file does not exist */
        need_sync = false;
        if (readonly || !initialize)
            EAccVerException::raise("DB file is not initialized. Run full update first");
    }

    m_Env.set_max_dbs(DBI_COUNT);
    m_Env.set_max_readers(THREAD_COUNT);

    int64_t     mapsize = MAP_SIZE_INIT;
    if (stat_rv == 0 && st.st_size + MAP_SIZE_DELTA >  mapsize)
        mapsize = st.st_size + MAP_SIZE_DELTA;

    m_Env.set_mapsize(mapsize);
    m_Env.open(db_path.c_str(),
               (readonly ?  MDB_RDONLY : 0) | MDB_NOSUBDIR |
               (need_sync ? 0 : (MDB_NOSYNC | MDB_NOMETASYNC)), 0664);
    m_IsReadOnly = readonly;

    if (initialize) {
        s_InitializeDb(m_Env);
    }

    auto    wtxn = lmdb::txn::begin(m_Env, nullptr, readonly ? MDB_RDONLY : 0);
    m_MetaDbi = lmdb::dbi::open(wtxn, META_DB, 0);
    wtxn.commit();

    m_Storage.reset(new CAccVerCacheStorage(m_Env));
}


void CAccVerCacheDB::Close()
{
    m_Env.close();
}


void CAccVerCacheDB::SaveColumns(const DDRPC::DataColumns &  clms)
{
    stringstream    os;
    clms.GetAsBin(os);

    auto    wtxn = lmdb::txn::begin(m_Env);
    m_MetaDbi.put(wtxn, STORAGE_COLUMNS, os.str());
    wtxn.commit();
}


DDRPC::DataColumns CAccVerCacheDB::Columns()
{
    auto        rtxn = lmdb::txn::begin(m_Env, nullptr, MDB_RDONLY);
    string      data;
    bool        found = m_MetaDbi.get(rtxn, STORAGE_COLUMNS, data);

    rtxn.commit();
    if (!found)
        EAccVerException::raise("DB file is not updated. Run full update first");

    stringstream        ss(data);
    DDRPC::DataColumns  rv(ss);
    return rv;
}
